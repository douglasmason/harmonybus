#!/usr/bin/env python3
"""Build the PDF from canonical Markdown and SVG; no separate prose copy.

Install docs-requirements.txt, then run from any directory. Each H2 starts a
page. Supported Markdown: paragraphs, bold, links, images and tables.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import re
import xml.etree.ElementTree as ElementTree
import json
import markdown
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, PageBreak, Table, TableStyle
from reportlab.pdfgen.canvas import Canvas
from svglib.svglib import svg2rlg

ROOT: Path = Path(__file__).resolve().parents[1]
FONT_ROOT: Path = Path('/usr/share/fonts/truetype/dejavu')
pdfmetrics.registerFont(TTFont('DejaVuSans',str(FONT_ROOT/'DejaVuSans.ttf')))
pdfmetrics.registerFont(TTFont('DejaVuSans-Bold',str(FONT_ROOT/'DejaVuSans-Bold.ttf')))
pdfmetrics.registerFontFamily('DejaVuSans',normal='DejaVuSans',bold='DejaVuSans-Bold',italic='DejaVuSans',boldItalic='DejaVuSans-Bold')

def inline(element: ElementTree.Element) -> str:
    """Translate Markdown HTML inline content into ReportLab paragraph markup."""
    result: str = ElementTree.tostring(element, encoding='unicode', method='xml')
    result = re.sub(r'^<[^>]+>|</[^>]+>$', '', result)
    result = result.replace('<code>', '<font name="Courier">').replace('</code>', '</font>')
    return result

def footer(canvas: Canvas, document: SimpleDocTemplate) -> None:
    """Draw common running furniture; ReportLab saves/restores our graphics state."""
    canvas.saveState()
    canvas.setFont('DejaVuSans-Bold',9);canvas.setFillColor(colors.HexColor('#007f83'))
    canvas.drawString(38,570,'HARMONYBUS / TIMING GUIDE')
    canvas.setFont('DejaVuSans',8);canvas.setFillColor(colors.HexColor('#526478'))
    canvas.drawString(38,22,'Generated from docs/timing-guide.md and editable SVG diagrams')
    canvas.drawRightString(804,22,str(document.page))
    canvas.restoreState()

def main() -> int:
    """Validate source references and render the shareable PDF."""
    parser: argparse.ArgumentParser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'dist/HarmonyBus-Timing-Guide.pdf')
    arguments: argparse.Namespace = parser.parse_args()
    source: Path = ROOT/'docs/timing-guide.md'
    contents: str = source.read_text()
    module: dict[str,object] = json.loads((ROOT/'modules/harmonybus/module.json').read_text())
    assert f"HarmonyBus {module['version']}" in contents, 'Guide version must match module version'
    html: str = markdown.markdown(contents,extensions=['tables'])
    tree: ElementTree.Element = ElementTree.fromstring('<document>'+html+'</document>')
    # GitHub-relative documentation links must also work in the exported PDF.
    for anchor in tree.iter('a'):
        address: str = anchor.get('href','')
        if not address.startswith(('https://','http://')):
            target: Path = (source.parent/address).resolve()
            assert target.is_file(), f'Missing linked source: {target}'
            anchor.set('href','https://github.com/douglasmason/harmonybus/blob/main/'+str(target.relative_to(ROOT)))
    body: ParagraphStyle = ParagraphStyle('body',fontName='DejaVuSans',fontSize=10,leading=14,spaceAfter=9,textColor=colors.HexColor('#13283c'))
    title: ParagraphStyle = ParagraphStyle('title',parent=body,fontName='DejaVuSans-Bold',fontSize=21,leading=25,spaceAfter=12)
    cell: ParagraphStyle = ParagraphStyle('cell',parent=body,fontSize=9,leading=12,spaceAfter=0)
    story: list[object] = []
    section_count: int = 0
    for element in tree:
        if element.tag=='h1': continue
        if element.tag=='h2':
            if section_count:story.append(PageBreak())
            section_count+=1
            story.append(Paragraph(inline(element),title))
        elif element.tag=='p':
            picture: ElementTree.Element | None = element.find('img')
            if picture is not None:
                diagram_path: Path = source.parent/picture.attrib['src']
                assert diagram_path.is_file(), f'Missing SVG: {diagram_path}'
                diagram = svg2rlg(str(diagram_path))
                assert diagram is not None
                scale: float = min(766/diagram.width, (175 if section_count==5 else 215)/diagram.height)
                diagram.scale(scale,scale)
                diagram.width*=scale;diagram.height*=scale
                story.extend([diagram,Spacer(1,10)])
            else:story.append(Paragraph(inline(element),body))
        elif element.tag=='table':
            map_rows_to_cells: list[list[Paragraph]] = [[Paragraph(inline(entry),cell) for entry in row] for row in element.iter('tr')]
            table: Table = Table(map_rows_to_cells,colWidths=[174,580],hAlign='LEFT')
            table.setStyle(TableStyle([('BACKGROUND',(0,0),(-1,0),colors.HexColor('#dce9ef')),('ROWBACKGROUNDS',(0,1),(-1,-1),[colors.HexColor('#edf3f7'),colors.white]),('VALIGN',(0,0),(-1,-1),'TOP'),('LEFTPADDING',(0,0),(-1,-1),8),('RIGHTPADDING',(0,0),(-1,-1),8),('TOPPADDING',(0,0),(-1,-1),7),('BOTTOMPADDING',(0,0),(-1,-1),7)]))
            story.extend([table,Spacer(1,12)])
        else:raise ValueError(f'Unsupported Markdown element: {element.tag}')
    arguments.output.parent.mkdir(parents=True,exist_ok=True)
    document: SimpleDocTemplate = SimpleDocTemplate(str(arguments.output),pagesize=(842,595),leftMargin=38,rightMargin=38,topMargin=44,bottomMargin=38,title='HarmonyBus Timing Guide',author='HarmonyBus',invariant=1)
    document.build(story,onFirstPage=footer,onLaterPages=footer)
    assert section_count==6
    print(arguments.output)
    return 0

if __name__=='__main__':
    raise SystemExit(main())
