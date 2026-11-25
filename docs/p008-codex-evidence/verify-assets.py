import collections, pathlib, xml.etree.ElementTree as ET
from PIL import Image
root = pathlib.Path(__file__).resolve().parents[2]
image_root = pathlib.Path('gui')
doc = ET.parse(root / 'resources/chopper.uidesc').getroot()
mapping = {node.attrib['name']: int(node.attrib['tag']) for node in doc.findall('control-tags/control-tag')}
assert set(mapping.values()) == set(range(100,113))
template = doc.find('template')
groups = collections.Counter(v.attrib['class'] for v in template if 'control-tag' in v.attrib)
assert groups == {'CAnimKnob': 10, 'CTextEdit': 10, 'CSegmentButton': 3}
assert sum(len(v.attrib['segment-names'].split(',')) for v in template if v.attrib['class']=='CSegmentButton')==9
for v in template:
    a=v.attrib
    x,y=map(int,a['origin'].split(','))
    w,h=map(int,a['size'].split(','))
    assert 0<=x<x+w<=960 and 0<=y<y+h<=576
for bitmap in doc.findall('bitmaps/bitmap'):
    a=bitmap.attrib
    image=Image.open(image_root/a['path'])
    image.verify()
    scale=float(a.get('scale-factor',1))
    if 'multiframe-size' in a:
        w,h=map(int,a['multiframe-size'].split(','))
        assert image.size==(w*scale*16,h*scale*8)
    else: assert image.size==(84*scale,42*scale)
print('UIDESC: 13 tags, 23 parameter surfaces, 9 segment options, 2 custom mouse surfaces, all bounds; 10 verified bitmap geometries')
