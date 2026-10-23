"""Authored camera/cast directions, shared by the archive builder and review tools.

Every shot names an actual feature of its painting. Cast is physical presence,
not a list of everybody who happens to speak later in the episode. A dash means
an insert without overlaid actors. PLATE scenes already contain their painted
characters and always use a dash. Radio callers are never placed in the scenery.
"""
from pathlib import Path
import json

CATALOG = json.loads((Path(__file__).resolve().parents[1]/'story/cast.json').read_text())
PEOPLE = [p[0] for p in CATALOG['people']]
CAST = PEOPLE + ['YOU','NARRATOR']
POSES = CATALOG['poses']
STAGES = ['FACE','PAN','RADIO','MONTAGE','PLATE']

def character(token):
    name, _, pose = token.partition('.')
    assert name in CAST and (not pose or pose in POSES), token
    assert name in PEOPLE or not pose, token
    return name, POSES.index(pose) if pose else 0

def load_story(root):
    root = Path(root)
    backgrounds = json.loads((root/'story/staging.json').read_text())['backgrounds']
    names = [b['name'] for b in backgrounds]
    scenes, current, shot = [], None, None

    def direction(words, lineno):
        assert len(words) == 5, (lineno, 'Expected BACKGROUND METHOD SUBJECT CAST FOCUS-POLICY', words)
        bg, stage, subject, present, policy = words
        assert policy in ('HOLD','MOVE'), (lineno, policy)
        assert bg in names and stage in STAGES, (lineno, bg, stage)
        b = backgrounds[names.index(bg)]
        assert subject in b['subjects'], (lineno, bg, subject)
        x, label = b['subjects'][subject]
        assert 0 <= x <= 512
        people = [] if present == '-' else [character(a) for a in present.split(',')]
        actors = [a[0] for a in people]
        assert len(actors) == len(set(actors)) and all(a in PEOPLE for a in actors), (lineno, actors)
        assert stage == 'FACE' or not actors, (lineno, 'Only a conversation shot stages physical actors')
        return {'background':names.index(bg), 'stage':STAGES.index(stage),
                'subject':subject, 'subject_label':label, 'subject_x':x,
                'focus':max(0,min(256,x-128)), 'present':actors, 'poses':dict(people),
                'caption_right':bg in names[6:] and list(b['subjects']).index(subject) == 1,
                'move':policy == 'MOVE', 'source_line':lineno, 'lines':[]}

    for lineno, raw in enumerate((root/'story/rising_tide.txt').read_text().splitlines(), 1):
        text = raw.strip()
        if not text or text.startswith('#'):
            continue
        if text.startswith('@'):
            key, *words = text[1:].split()
            assert 'DCBAS'.index(key[0])*25+int(key[1:])-1 == len(scenes), (lineno,key)
            current = {'id':key, 'lines':[], 'shots':[]}
            scenes.append(current)
            shot = direction(words, lineno)
            current['shots'].append(shot)
        elif text.startswith('>'):
            assert shot and shot['lines'], (lineno, 'An empty shot is probably an editing mistake')
            shot = direction(text[1:].split(), lineno)
            current['shots'].append(shot)
        else:
            token, message = text.split(':', 1)
            who, override = character(token)
            message = message.strip()
            assert current and shot and who in CAST, (lineno,who)
            assert message.isascii() and 0 < len(message) < 192, (lineno,len(message),message)
            speaker = CAST.index(who)
            assert shot['stage'] != 0 or speaker >= len(PEOPLE) or who in shot['present'], (lineno, 'Speaker absent from physical shot', who)
            # Actor changes never introduce someone from another location or a radio.
            last = next((l['speaker'] for l in reversed(shot['lines']) if l['speaker'] < len(PEOPLE)), None)
            others = [CAST.index(a) for a in shot['present'] if a != who]
            companion = last if last in others else next(iter(others),255)
            line = {'speaker':speaker,'companion':companion,'text':message,
                    'background':shot['background'],'stage':3 if who == 'NARRATOR' else shot['stage'],
                    'pose':override if '.' in token else shot['poses'].get(who,0),
                    'companion_pose':shot['poses'].get(CAST[companion],0) if companion != 255 else 0,
                    'focus':shot['focus'], 'move':shot['move'], 'caption_right':shot['caption_right'], 'shot':len(current['shots'])-1}
            shot['lines'].append(line)
            current['lines'].append(line)
    assert len(scenes) == 125 and shot['lines']
    assert all(4 <= len(s['lines']) <= 16 for s in scenes)
    return backgrounds, scenes
