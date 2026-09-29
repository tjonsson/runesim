import unittest
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from osm_route_candidates import classify,convert

class RouteCandidateTests(unittest.TestCase):
    def test_road_is_not_a_sidewalk(self):
        self.assertIsNotNone(classify({'highway':'residential','sidewalk':'both'},'walk'))
        self.assertIsNone(classify({'highway':'footway','footway':'sidewalk'},'walk'))
    def test_access_precedence(self):
        self.assertIsNotNone(classify({'highway':'residential','access':'private'},'drive'))
        self.assertIsNone(classify({'highway':'residential','access':'private','motorcar':'yes'},'drive'))
        self.assertIsNotNone(classify({'highway':'residential','access':'yes','motor_vehicle':'no'},'drive'))
        self.assertIsNotNone(classify({'highway':'footway','foot':'use_sidepath'},'walk'))
    def test_uncertain_ways_excluded(self):
        for key,value in [('access:conditional','yes @ (Mo-Fr)'),('bridge','yes'),('tunnel','yes'),('ford','yes'),('indoor','yes'),('area','yes'),('level','1'),('layer','-1'),('oneway','alternating'),('motor_vehicle:forward','no'),('building','yes'),('natural','water')]:
            with self.subTest(key=key):self.assertIsNotNone(classify({'highway':'residential',key:value},'drive'))
    def test_reverse_oneway_preserves_provenance(self):
        data={'elements':[{'type':'way','id':123,'tags':{'highway':'residential','oneway':'-1'},
                          'geometry':[{'lon':18,'lat':59},{'lon':18.001,'lat':59}]}]}
        result,_=convert(data,20)
        feature=result['features'][0]
        self.assertEqual(feature['geometry']['coordinates'][0],[18.001,59,20])
        self.assertTrue(feature['properties']['one_way'])
        self.assertFalse(feature['properties']['validated'])
        self.assertEqual(feature['properties']['license'],'ODbL-1.0')
        data['elements'][0]['tags']['oneway:motorcar']='no'
        self.assertFalse(convert(data,20)[0]['features'][0]['properties']['one_way'])
    def test_invalid_geometry_fails_closed(self):
        for geometry in ([],[{'lon':18,'lat':59}]*2,[{'lon':float('nan'),'lat':59},{'lon':18,'lat':59}]):
            data={'elements':[{'type':'way','id':1,'tags':{'highway':'footway'},'geometry':geometry}]}
            self.assertFalse(convert(data,0)[0]['features'])
        with self.assertRaises(ValueError):convert({},float('inf'))

if __name__=='__main__':unittest.main()
