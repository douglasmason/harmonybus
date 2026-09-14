/* Export actual host metadata for JSON/controller validation. */
#define main motion_regression_main
#include "chord_player_test.c"
#undef main
int main(void){
    Inst *instance=fixture();char metadata[65536];
    assert(API.get_param(instance,"chain_params",metadata,sizeof(metadata))>0);puts(metadata);
    API.set_param(instance,"motion_host","movy-clip-v1");
    assert(API.get_param(instance,"chain_params",metadata,sizeof(metadata))>0);puts(metadata);
    API.set_param(instance,"motion_operation","Clip Reverse");API.set_param(instance,"motion_host","");
    assert(API.get_param(instance,"chain_params",metadata,sizeof(metadata))>0);puts(metadata);
    return 0;
}
