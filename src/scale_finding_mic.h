/* Scale Finding's exclusively enabled host-input chromagram.
 * No allocation or libm at audio rate. Only the conductor MIDI-FX instance
 * reads Schwung's existing 128-frame stereo signed-16 input mailbox.
 * When the interface leaves Scale Finding, it switches the atomic gate off;
 * the next tick stops reading input and clears the published strengths.
 */
#define SF_MIC_WINDOW 2048u
#define SF_MIC_DECIMATE 4u
#define SF_MIC_BINS 12u
#define SF_MIC_NOTES 36u
typedef struct {
    float ring[SF_MIC_WINDOW];
    unsigned write, count, since;
    unsigned active_generation;
    unsigned char strengths[SF_MIC_BINS];
} sf_mic_state;
static sf_mic_state g_sf_mic_state;
static int g_sf_mic_owner = -1;
static unsigned g_sf_mic_generation = 1;
static int g_sf_mic_enable = 0;
/* Goertzel coefficients for MIDI 48..83, 11,025Hz (44.1kHz / 4).
 * Fixed frequencies are generated at build time, never computed on Move. */
static const double SF_MIC_COEFF[SF_MIC_NOTES] = {
    1.994444774, 1.993764823, 1.993001696, 1.992145231, 1.991184027, 1.990105296,
    1.988894693, 1.987536128, 1.986011559, 1.984300750, 1.982381015, 1.980226920,
    1.977809955, 1.975098169, 1.972055760, 1.968642621, 1.964813829, 1.960519089,
    1.955702099, 1.950299861, 1.944241911, 1.937449466, 1.929834489, 1.921298655,
    1.911732219, 1.901012779, 1.889003922, 1.875553768, 1.860493383, 1.843635097,
    1.824770699, 1.803669548, 1.780076608, 1.753710434, 1.724261155, 1.691388522
};
static void sf_mic_clear_audio(void) {
    g_sf_mic_state.write=0;
    g_sf_mic_state.count=0;
    g_sf_mic_state.since=0;
    for(unsigned index=0;index<SF_MIC_BINS;index++)
        __atomic_store_n(&g_sf_mic_state.strengths[index],0,__ATOMIC_RELEASE);
}
/** Called only on UI/user control writes; never clear the DSP's private ring
 * from another thread. The tick observes an atomic generation change. */
static void sf_mic_set(Inst *instance,const char *parameter) {
    if(!instance||instance->role!=0)return;
    int next=!strcmp(parameter,"1")||!strcmp(parameter,"On");
    if(next){
        __atomic_store_n(&g_sf_mic_owner,(int)(instance-g_pool),__ATOMIC_RELEASE);
        __atomic_store_n(&g_sf_mic_enable,1,__ATOMIC_RELEASE);
    }else if(__atomic_load_n(&g_sf_mic_owner,__ATOMIC_ACQUIRE)==(int)(instance-g_pool)){
        __atomic_store_n(&g_sf_mic_enable,0,__ATOMIC_RELEASE);
    }
    __atomic_add_fetch(&g_sf_mic_generation,1,__ATOMIC_ACQ_REL);
}
static void sf_mic_analyze(void) {
    /* A time-domain noise gate precedes all pitch extraction. */
    double power=0.0,energy[SF_MIC_BINS]={0};
    for(unsigned index=0;index<SF_MIC_WINDOW;index++){
        double sample=g_sf_mic_state.ring[index];
        power+=sample*sample;
    }
    if(power>=0.00001*SF_MIC_WINDOW){
        for(unsigned pitch=0;pitch<SF_MIC_NOTES;pitch++){
            double prev=0.0,older=0.0,coefficient=SF_MIC_COEFF[pitch];
            for(unsigned index=0;index<SF_MIC_WINDOW;index++){
                double sample=g_sf_mic_state.ring[(g_sf_mic_state.write+index)&(SF_MIC_WINDOW-1u)];
                double state=sample+coefficient*prev-older;
                older=prev;prev=state;
            }
            double estimate=prev*prev+older*older-coefficient*prev*older;
            if(estimate>0)energy[pitch%12]+=estimate;
        }
    }
    double strongest=0.0,sum=0.0;
    for(unsigned pc=0;pc<SF_MIC_BINS;pc++){
        if(energy[pc]>strongest)strongest=energy[pc];
        sum+=energy[pc];
    }
    /* Avoid asserting a key from flat noise or microphone preamp hiss.
     * All twelve notes use one shared normalization. */
    int voiced=strongest>0.01&&strongest*SF_MIC_BINS>sum*1.6;
    for(unsigned pc=0;pc<SF_MIC_BINS;pc++){
        int target=voiced?(int)(energy[pc]*120.0/strongest+0.5):0;
        if(target>120)target=120;
        int previous=__atomic_load_n(&g_sf_mic_state.strengths[pc],__ATOMIC_RELAXED);
        int smoothed=(previous*2+target)/3;
        if(smoothed<2)smoothed=0;
        __atomic_store_n(&g_sf_mic_state.strengths[pc],(unsigned char)smoothed,__ATOMIC_RELEASE);
    }
}
static void sf_mic_tick(Inst *instance,int frames,int sample_rate) {
    if(!instance)return;
    unsigned generation=__atomic_load_n(&g_sf_mic_generation,__ATOMIC_ACQUIRE);
    if(g_sf_mic_state.active_generation!=generation){
        sf_mic_clear_audio();
        g_sf_mic_state.active_generation=generation;
    }
    if(!__atomic_load_n(&g_sf_mic_enable,__ATOMIC_ACQUIRE)||
       __atomic_load_n(&g_sf_mic_owner,__ATOMIC_ACQUIRE)!=(int)(instance-g_pool)||
       !g_host||!g_host->mapped_memory||g_host->audio_in_offset<0||
       sample_rate!=44100||frames<=0)return;
    const signed short *audio=(const signed short *)(g_host->mapped_memory+g_host->audio_in_offset);
    unsigned bound=(unsigned)frames>128u?128u:(unsigned)frames;
    for(unsigned frame=0;frame<bound;frame+=SF_MIC_DECIMATE){
        /* Average stereo channels once. No monitor/routing is enabled here. */
        int sample=((int)audio[frame*2]+(int)audio[frame*2+1])/2;
        g_sf_mic_state.ring[g_sf_mic_state.write]=(float)sample/32768.0f;
        g_sf_mic_state.write=(g_sf_mic_state.write+1u)&(SF_MIC_WINDOW-1u);
        if(g_sf_mic_state.count<SF_MIC_WINDOW)g_sf_mic_state.count++;
        g_sf_mic_state.since++;
    }
    if(g_sf_mic_state.count==SF_MIC_WINDOW&&g_sf_mic_state.since>=1024u){
        g_sf_mic_state.since=0;
        sf_mic_analyze();
    }
}
static int sf_mic_get(Inst *instance,char *buffer,int length) {
    if(length<60)return -1;
    if(!instance||!__atomic_load_n(&g_sf_mic_enable,__ATOMIC_ACQUIRE)||
        __atomic_load_n(&g_sf_mic_owner,__ATOMIC_ACQUIRE)!=(int)(instance-g_pool))
        return snprintf(buffer,(size_t)length,"off");
    int used=snprintf(buffer,(size_t)length,"mc1");
    for(unsigned pc=0;pc<SF_MIC_BINS;pc++){
        unsigned level=__atomic_load_n(&g_sf_mic_state.strengths[pc],__ATOMIC_ACQUIRE);
        used+=snprintf(buffer+used,(size_t)(length-used),",%u",level);
    }
    return used;
}
