"""Prevent holdout leakage when a successful head becomes the next teacher."""
import unittest
from curriculum import position_key, select_rows


def row(key, family, split):
    return {'key':key,'family':family,'split':split,'source':'fresh'}


class CurriculumTests(unittest.TestCase):
    def test_holdouts_and_old_families_never_enter_training(self):
        previous=[row('train','old-train',0),row('val','old-val',1),row('test','old-test',2)]
        fresh=[row('val','new-fam',0),row('new','old-test',0),
               *[row(str(i),'new-train',0) for i in range(3)],row('v','new-val',1),row('t','new-test',2)]
        selected,stats=select_rows(fresh,previous,.25,7)
        self.assertEqual(stats['historical_overlap_rows_excluded'],2)
        self.assertEqual(stats['replay_train_rows'],1)
        self.assertEqual({r['key'] for r in selected if r['source']=='replay'},{'train'})
        self.assertFalse({'val','test','new'} & {r['key'] for r in selected})
        self.assertEqual(stats['replay_fraction_actual'],.25)

    def test_cross_split_duplicates_are_removed_from_every_split(self):
        rows=[row('collision','train',0),row('collision','val',1),
              row('one','train',0),row('one','train',0),row('two','test',2)]
        selected,stats=select_rows(rows)
        self.assertEqual({r['key'] for r in selected},{'one','two'})
        self.assertEqual(stats['cross_split_duplicates_removed'],2)
        self.assertEqual(stats['within_split_duplicates_removed'],1)

    def test_replay_is_reproducible_without_reusing_holdout_labels(self):
        old=[row(str(i),'old-family',0) for i in range(20)]
        new=[row('new'+str(i),'new-family',0) for i in range(9)]
        first,_=select_rows(new,old,.25,123)
        second,_=select_rows(new,list(reversed(old)),.25,123)
        self.assertEqual(first,second)
        self.assertEqual(sum(r['source']=='replay' for r in first),3)
        with self.assertRaises(ValueError): select_rows(new,old[:1],.25,123)

    def test_score_and_rule50_are_part_of_deduplication_key(self):
        fen='8/8/8/8/8/8/k7/7K w - - 2 9 points=3,4'
        self.assertEqual(position_key(fen),position_key(fen.replace(' 9 ',' 19 ')))
        self.assertNotEqual(position_key(fen),position_key(fen.replace('points=3,4','points=4,3')))
        self.assertNotEqual(position_key(fen),position_key(fen.replace(' 2 9 ',' 3 9 ')))


if __name__=='__main__': unittest.main()
