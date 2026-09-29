import unittest,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from osm_exclusions import envelopes,first_conflict,intersects_box
from osm_route_candidates import convert

class ExclusionTests(unittest.TestCase):
    def test_segment_crossing_without_inside_endpoints(self):
        self.assertTrue(intersects_box((-2,0),(2,0),(-1,-1,1,1)))
        self.assertTrue(intersects_box((0,0),(.5,.5),(-1,-1,1,1)))
        self.assertTrue(intersects_box((-2,1),(2,1),(-1,-1,1,1)))
        self.assertFalse(intersects_box((-2,2),(2,2),(-1,-1,1,1)))
    def test_clearance_and_relation_gaps_are_conservative(self):
        water={'type':'relation','id':9,'tags':{'natural':'water'},'members':[{'geometry':[{'lon':0,'lat':0},{'lon':.001,'lat':.001}]}]}
        blocked=envelopes({'elements':[water]})
        self.assertIsNotNone(first_conflict([[-.001,-.000005],[.002,-.000005]],blocked,1.))
        self.assertIsNone(first_conflict([[-.001,-.0001],[.002,-.0001]],blocked,1.))
    def test_incomplete_exclusions_reject_export(self):
        with self.assertRaises(ValueError):envelopes({'elements':[{'type':'way','tags':{'building':'yes'}}]})
    def test_converter_excludes_water_and_keeps_candidate_unapproved(self):
        water={'type':'way','id':9,'tags':{'natural':'water'},'geometry':[{'lon':0,'lat':0},{'lon':.001,'lat':.001}]}
        path={'type':'way','id':10,'tags':{'highway':'footway'},'geometry':[{'lon':-.001,'lat':.0005},{'lon':.002,'lat':.0005}]}
        candidates,rejected=convert({'elements':[path,water]},0)
        self.assertFalse(candidates['features'])
        self.assertTrue(any('water exclusion' in r['reason'] for r in rejected))
        path['geometry']=[{'lon':-.001,'lat':.002},{'lon':.002,'lat':.002}]
        candidates,_=convert({'elements':[path,water]},0)
        self.assertFalse(candidates['features'][0]['properties']['validated'])
        self.assertEqual(candidates['features'][0]['properties']['exclusion_prefilter']['supplied_envelopes'],1)

if __name__=='__main__':unittest.main()
