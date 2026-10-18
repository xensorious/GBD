"""Visual-overhaul regression checks against the released First Light prototype.

Run after ``make`` with ``python3 tests/visual_validation.py``. The emulator is
driven only with buttons; reading symbols observes state, never changes RAM.
Reference hashes preserve the original gameplay even when no baseline checkout
is available. ``--baseline`` can additionally compare a saved game.c directly.
"""
from pathlib import Path
import argparse
import ctypes as C
import hashlib
import re

import playthrough as p
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
CONFIRM = 11
VISUAL_FUNCTIONS = {
    "rect", "panel", "text", "smallnum", "center", "wrap", "art",
    "creature", "person", "ellipse", "bar", "marker", "render",
    "drawfield", "drawdialog", "drawtitle", "drawchoose", "drawmenu",
    "drawteam", "drawgrowth", "drawguide", "drawjournal", "drawbattle",
    "drawending",
}

# These are normalized C-token hashes from the released, pre-overhaul game.
# Formatting/comments do not matter; expressions, content, state and logic do.
REFERENCE = {'declarations': ['dffbc7213b8918c542cedbd3c32ce70d67a4a600c8833497419865788be7537e',
                  '718b981450d42e9b064de6b205818d8d0f5e72fc3cb491063a8603b6fd6f3912',
                  'e98041e04a0b7d328a9ca17af90e67a3de1358fed1ea73ac57622720df63cc8a',
                  '01bdc1fe2a732ba4eba21ef733df6b48e6f18ba3a0a16b32786c8634aebccf0f',
                  '7378c525e1fefe7ae7051d03150395b542b5bb5e391227f1faeec1dbfc733f50',
                  'b18574658567946adeb6e1f93ee82001e1f4a3b50b401665d74e7c7313626d4a',
                  'ce5f6cd4d82114aa75f804d592f8734b5d0486c784c0816f5475da35c6b9ef4a',
                  '4ba9f87f705bf53c8b7cbb7aef384b8700956af87adbf2ce3a8fc1b3fa8073eb',
                  '3fd7b067c18d84eaeb6e2c903b7f07ba95266ec416b9f2764004261c2e873fb5',
                  'ce46ae1942689c106d3d1aded0eee7f8d57f030a5063759c136567fb4cd06188',
                  'edcb2b9f6988dfd088dc2ca394c390e3dd133ba1b502de636008438293822413',
                  '0c5eae5d648674876a7ab5121c9b000915570c38b33df88eb3e4571736fe4d6e',
                  '20327c5ce0ac66679c40124cea216c2f2e775027cacda110ffc2470f23ab593b',
                  '79a559488d407d2e49459305b25720d8885a11ce47fab9e9bf0e0fc0c5aef0ab',
                  '0f9cbea2f87f7e8d6cb16c2cd7a7a6ad2a1bb5294aa015d683bb1bbef2f3a2de',
                  '0006eac9d576f47d1b9f2c5f877fba5c8e659e2d8a95c3ff6b98e062e94645a9',
                  '56d3ec0171c05c543f5b0b4f2f6f10d987f3d9d9d4777918a6f83b0fc13e2872',
                  '0aa913e534592e1126f378028fee0f5f36101f286bb3d6d780a197f6661a1c0d',
                  '51050d5b8d2d3a70e39691edf9f927cc0a45d06d291310fc9504a264d24a3de2',
                  'f4b271add4f30680f4b9be45d9bee1d8a07ff1cf21ac85830791f5602ccbc85d',
                  '82975627875f9b3bb4ec8793a51b52cc895f7227a60bb625db33055e9f8a746e',
                  '4ecf721c9715a18d8972b55926f5a2bf24055ae61771e39ea4d355b60327733f',
                  'b10db598f441188dd7f237d46801efe4d418f5c854d19881d98217d28e9951dc',
                  '74b69a8587979e389d5821901e92ddd0e8e40d62e7109ff175672218839ffc3b',
                  'd29f602f1c4508b154d8f5a26c329945d60fd907d920c9f53ad287617d0ec5ab',
                  '0fc7dde0c2dae413d59ff4dfc4b0df879f2873699c78e6f5c26e0ff184421850',
                  '60b280776b0e586ec97e549399603eb5f55b50df9dd82c2406ce4378471b45df',
                  '47be6b412b412262091de65ef7162e96bd1b95a96003a3bcd356047e1cd0af12',
                  '190372e64cd7c4e8b277209a5cc56f9111931ac2cb3628eb42200c1ce9b42009',
                  '47ca5d5be59957cf46dc388905098582caa05bbf6cb4c1546d9079c67732cf08',
                  'f3bb9c247168960d3dd11999431c8832af462f341d40d9792be9de73025d32ed',
                  '6e1e9c4181693ac7c920ffe4a07bc60442625e158afbe41ff0d58d36edbc4af0',
                  '3948410f9780a708e4e7e2166db60a73b039e5c4fe8bfd255114aa3bb5d7f3a8'],
 'defines': {'CLAMP': '1459057389f0f77d3f5f9b65efccf0dc0dd06f762714dc6ddede1c027fbd034c',
             'MAX': '38cda0b360c8a4e63f4b1c7ba9e7fc0db375b2200c807cfa2be92382297e2d98',
             'MIN': '2ff55f49db857b2a852618e9800ad6c936513434cb78975fec51c89387055971',
             'REG16': '1371bb055fe19c309031043bc8d83a967bd26f8acf1259ca84620ce7020e4b6b',
             'REG32': 'a1d71b294bbd7167cee9a23b19aae63e0d85e1c685f0be39b37bb9b783278b27'},
 'functions': {'__aeabi_memclr': '7827ee2607b5010a1ea9f413458232e239edf20eab556f6433033b52d0c1724c',
               '__aeabi_memclr4': 'f2555eb50cd97d7aa2b90ac68731a06a84686a660f32a0220b94c2c579326d64',
               '__aeabi_memcpy': 'dbc833ab4e6a70cfd8319b529944adf947c8337256379cb99fe7de5e86f86c9b',
               '__aeabi_memcpy4': 'b636434e1299f540c8c4ba551e52fd9df9cde48688be5116c70b074c442310d5',
               'absval': 'aa40154bae2e39a4b6e6d478e8bcd3bbd9b33f83b8b9b451dd67001b4d8d4389',
               'addnum': '6337d482f3bcb0af9c8340817094222f218fe2bddfe99c4986a43a6f7e2d34df',
               'afterbattle': '896079df783d0e46a81ff63544f57a324a5d42a5b96cac0be4f24ae0b135f88c',
               'append': 'b2b5d2ed379dcac0a2782f5818acf419ccf67c7844b2af0fd63a2b4e4fe78e36',
               'audio': '79e1e903c171a8eb0655f8bbd589c20ed7b3c13b68943dc5cd227ffed7859ce7',
               'available': '8ee2851e904fb036a397f23daf76dbb957d1f1a5d83c75d48eefaf5481e44328',
               'battledamage': '85cc78eb79d95d2047ba484886066af838c073a184e6dd1dd47a301deae2533b',
               'battleinput': '4a68238591131bfd4066af941a5181701e98e20c07957486f45d2f154011d18c',
               'battlelog': 'b7c251ef02f120d738e34215247e2f3c16fb311bd0c37f32aae7438fc8456532',
               'blocking': '0f9e09e8cf3823daf3f18d6287dc2956aac0b3f19901950abace2a53996f8c53',
               'checksum': 'd922fd484cf726625a2bb41cc7e1aa720a52392592b4885d5df14b6c568241c1',
               'choosepartner': '4ed28912c7307c0c6ac37a9f6807ae6f3f84af4199b302224a49ebd2ad837d5f',
               'copy': 'df456bce8aaf1b8286db41effedaddd530adc51221f027bfc13377aae261a255',
               'damage': '098a1472fc7d39faeb68f697c061e5abbca31ec1e6173c3e1431098e681a15f6',
               'defense': '318f893657c5c786cad186e64457ccf7f92396af22850a8c7e4eb29603a79f89',
               'dialogue_input': 'aded14f702f94f27148ea2fb3afff52717b7eb81fcf1157c8d53a96c5a97412d',
               'effectiveness': '552561668ce2bb613e69b98a54b8a8c422693a2ab8e5c34d7d32d82311648d79',
               'enemyturn': 'bb3270d8e616ac998e8f968ea6ca0688101e3257db2a5171098657839c9e0697',
               'fieldcamp': '80e3315bfab6da93fd113179715269686a2df894e523d1ee90959bfadfb521b8',
               'fieldinput': '89b60143257d3d919f1935c314a84de712d9faae901218e268042ebd1561e3c1',
               'finishbattle': 'f5fc40cdd2aeb5eb855ba47ef28b6ae8aa18dc8e1e92a44b5353d11fcf31b50b',
               'firstalive': '64c89fb1ce6df4055c4f570345ace105583620ba06013f44d194ca17466448f3',
               'givexp': 'e2fb6fb6a37c3a17dff5df072d20f23bfc472abb3526445faf619150bedc856a',
               'growthinput': 'c1ee039f6759d923548d23a4112f812b020ca9afe11b93d1ce6c6b4e6b11f543',
               'heal': '75edb5adb2c8c63fa3953d0c16e2e535a2cbcdef6d1c3cfe2fac54668ef0f055',
               'healall': 'f8e967d43c64474c40751d2279b8b92e143722fe4aecda2f431ba832fd97f978',
               'initworld': '87ee10f4d35ccb8edfd721fff7240deb8c5ffb90b4cae2a210946f605fb3a15f',
               'input': 'f26fdf3d56ef1bdad2ccbe909b62c44a2630a5c0d4b81566750cc44243b81d2c',
               'length': '477581e43052106bee5bea52a668a8be9363451866ff248d24cd5e45c62969ae',
               'loadgame': '37f1ad11c1bfcea7a90bea8f12af5834457ff06ae3322f4a2788de32cff61031',
               'lognamed': '335e7b345995518e42e7d2551e4739f14980c464115e5d1f4d9e1b8fd3e25473',
               'main': 'ef5b126ac710f78bdd8a43bcf545359a0c399f0bb6d6690dc8db8175128777a3',
               'makefriend': '249f4ebc59fc568d1edc6a6e0094e9ba5e43d67303de178cac698d64938451fc',
               'maxenergy': '446920c4333b75257d9cff1da2e0a71ae89529565c503eb44e31d2b89bf51c4b',
               'maxhp': '63fe3f9afd90f8edb5d80ae98ae5c9ae2f00f3f85fb02656201313985dc41680',
               'memcpy': 'c15cb327e702370c65eb662b58fd791748eacfe79ec6036d9c3606bb3054d12d',
               'memset': '5b7ac35bd081984fda820191a7116525eeac162f67a8272257d4fe6b5ec890c6',
               'menuinput': '4c862d75bf9ff1a671e83c5924eccd35d5821a7f38d8f07ea567e62d4620285a',
               'movename': 'ee0fcedf11acabd5bb0cc2fa006fbd36a14a0863a8ff35a45af28f75cd887243',
               'near': '3f3a111c04b9cdbf03eff8b9d9a6e82d37387504a5d74d0744dc95db2b19d0f4',
               'newgame': '59d5662080830f9aba73fa6a89495ffad520393b19935074cd2afdbb9afea803',
               'npcblock': 'd77aabda9e57e80b9e3db5cae5f00c3500f00b12e24746a8cd8b5b25a6228482',
               'number': '87ea5cdff28cfd8fa8c61efe348c955107a5b905c662c6fd9ff080f560121f0a',
               'objective': '8cfbfd658a40e1a7bff94eb7844786e357a2745f9b972b06d748af7267a11c68',
               'owned': 'ef1dafaf6f2ec08b61f5d41bda8052b3b8c29c619764a018506bb69ea03f1cc4',
               'power': '78a16296b07e5a09bd577748be2f581bb70c5600822aeeaceb8e9f647b03e1a5',
               'readslot': 'cf43d4a8c89b36ca7046ec20ccb7d54ed34694ec89621aa6fb43a40344025ba2',
               'recruit': 'af04c38ce18e2dad34858fff40ee8bdf4c70fa7593396f0e17eac92ae4ab0c8f',
               'savegame': 'd994384ce097842f4d7f910ac783b1f834847c4c2f6931ffe7d65ca14e512348',
               'say': 'e80340ba8636307e5223cdcc1e38941fcce10ed17110ded65c1ba79bbfb686a2',
               'showgrowth': 'c1546ba8ede07e83d695da9341e356fa8305629c8aa4edaacadf2f8f3c56402b',
               'startbattle': '1f9d185fd6cf46199858053f1eb61474204aa72fbb36d38fa4a1c62b53d92bc9',
               'talk': 'ab9449831213bada7aa8dbfe77b06cdb623a195ec709ea0e7f241294ad39e6a2',
               'teaminput': '9b970986e6dd267895cf315098d21d47940bdb20f89f1c36ddd4cfeb510ffd1e',
               'tone': '17f7a91a907afdbcc0b9578b2bf884969236854e55b447579a36bb132318a6e1',
               'usemove': 'fe022f6c254cf302f4cecc1d00c70b0e2bf73d8bfee3269d2640dec0f3870211',
               'valid': 'e42cdd861fc3b24c0dc846578d55aa242693a551bf10ae00e88c1f2d41e338ca'},
 'map_tiles': '1e75766d210b786706009780561e3169172e15dc6c7fb4fb3f39510ea38fbb7d'}

TOKEN = re.compile(
    r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|'
    r"'(?:\\.|[^'\\])*'|[A-Za-z_]\w*|0[xX][0-9A-Fa-f]+[uUlL]*|"
    r"\d+[uUlL]*|==|!=|<=|>=|->|\+\+|--|&&|\|\||<<|>>|\+=|-=|\*=|/=|[^\s]"
)


def tokens(source):
    source = re.sub(r"^\s*#.*$", "", source, flags=re.M)
    return [m.group() for m in TOKEN.finditer(source)
            if not m.group().startswith(("//", "/*"))]


def digest(value):
    if isinstance(value, str):
        value = value.encode()
    return hashlib.sha256(value).hexdigest()


def c_parts(source):
    """Extract top-level functions/declarations, honoring quoted braces."""
    ts = tokens(source)
    functions, declarations = {}, []
    start = i = 0
    while i < len(ts):
        if ts[i] == "{":
            is_function = i > start and ts[i - 1] == ")"
            name = None
            if is_function:
                level, k = 1, i - 2
                while level:
                    if ts[k] == ")": level += 1
                    elif ts[k] == "(": level -= 1
                    k -= 1
                name = ts[k]
            depth, j = 1, i + 1
            while depth:
                if ts[j] == "{": depth += 1
                elif ts[j] == "}": depth -= 1
                j += 1
            if name is not None:
                assert name not in functions, ("duplicate C function", name)
                functions[name] = ts[start:j]
                start = j
            i = j
        elif ts[i] == ";":
            declarations.append(ts[start:i + 1])
            i += 1
            start = i
        else:
            i += 1
    assert not ts[start:], ("unparsed C", ts[start:])
    return functions, declarations


def joined_source(path):
    """Follow renderer include files while leaving generated assets separate."""
    source = path.read_text()
    for include in re.findall(r'^\s*#include\s+"([^\"]+\.inc)"', source, re.M):
        source += "\n" + joined_source(path.parent / include)
    return source


def array_values(header, name):
    found = re.search(r"\b" + re.escape(name) + r"\[\d+\][^=;]*=\s*\{([^}]+)\}", header)
    assert found, ("missing generated array", name)
    return list(map(int, re.findall(r"\d+", found.group(1))))


def array_bytes(header, name):
    return bytes(array_values(header, name))


def fingerprints(source, map_data):
    functions, declarations = c_parts(source)
    return {
        "functions": {name: digest("\n".join(body))
                      for name, body in functions.items()
                      if name not in VISUAL_FUNCTIONS},
        "declarations": [digest("\n".join(decl)) for decl in declarations],
        "defines": {match.group(1): digest("\n".join(tokens(match.group(0).replace("#", "", 1))))
                    for match in re.finditer(r"^\s*#define\s+(\w+)[^\n]*", source, re.M)},
        "map_tiles": digest(map_data),
    }


def static_checks(baseline=None):
    source = joined_source(ROOT / "src/game.c")
    functions, declarations = c_parts(source)
    current = fingerprints(source, array_bytes((BUILD / "assets.h").read_text(), "map_tiles"))
    reference = REFERENCE
    if baseline:
        old_functions, old_declarations = c_parts(Path(baseline).read_text())
        assert {n: digest("\n".join(v)) for n, v in old_functions.items()
                if n not in VISUAL_FUNCTIONS} == reference["functions"]
        assert [digest("\n".join(v)) for v in old_declarations] == reference["declarations"]
    changed = [name for name, expected in reference["functions"].items()
               if current["functions"].get(name) != expected]
    assert not changed, ("gameplay functions changed", changed)
    # New renderer helpers/globals are allowed; every original declaration,
    # including Save/Friend/Battle layout and species/encounter data, is exact.
    missing = [n for n, expected in enumerate(reference["declarations"])
               if expected not in current["declarations"]]
    assert not missing, ("original globals/content/layout changed", missing)
    assert all(current["defines"].get(name) == value
               for name, value in reference["defines"].items()), "original hardware/math macros changed"
    assert current["map_tiles"] == reference["map_tiles"], "logical collision maps changed"
    print("PASS: original gameplay functions, data/layout and logical maps are unchanged")


class VisualEmulator(p.Emulator):
    captures = []

    def screenshot(self, name):
        if not name.startswith("visual-"):
            name = "visual-" + name
        super().screenshot(name)
        path = BUILD / (name + ".png")
        if path not in self.captures:
            self.captures.append(path)


def dialogue_pages(e, name):
    page = 0
    while e.mode == p.DIALOG:
        e.screenshot(f"{name}-{page}")
        e.tap(p.A)
        page += 1
        assert page < 50, "dialogue failed to end"


def renderer_pace(e, label):
    """Observe actual GBA-cycle presentation rate, independent of host speed."""
    before = e.read(e.sym["frame"], 4)
    e.frames(120)
    rendered = e.read(e.sym["frame"], 4) - before
    assert rendered > 0, ("renderer stalled", label)
    if label == "field":
        assert rendered >= 60, "field rendering slowed the original 30 FPS movement"
    print(f"Renderer pace, {label}: {rendered} updates / 120 emulated video frames")


class SceneObject(C.Structure):
    _fields_ = [("x", C.c_int16), ("y", C.c_int16),
                ("depth", C.c_int16), ("kind", C.c_uint16)]


def occlusion_checks(e):
    """Prove actual overlap pixels change owner on opposite sides of a house.

    Unchanged collision tiles let the player stand north of its roof and south
    of its wall. We use only normal walking, then compare framebuffer pixels
    where two different, opaque sprite colors overlap. This cannot pass merely
    because sorting metadata claims to be correct.
    """
    header = (BUILD / "assets.h").read_text()
    starts = array_values(header, "scene_starts")
    raw = re.search(r"scene_objects\[\]\s*=\s*\{([\s\S]*?)\};", header).group(1)
    objects = [SceneObject(*map(int, re.findall(r"-?\d+", row)))
               for row in re.findall(r"\{([^}]+)\}", raw)]
    objects = objects[starts[0]:starts[1]]
    house = next(o for o in objects if (o.x, o.y, o.depth) == (56, 72, 160))
    widths = array_values(header, "scenery_widths")
    heights = array_values(header, "scenery_heights")
    offsets = array_values(header, "scenery_offsets")
    scenery = array_bytes(header, "scenery_art")
    people = array_bytes(header, "people_art")
    width, height = widths[house.kind], heights[house.kind]
    art = scenery[offsets[house.kind]:offsets[house.kind] + width * height]
    for x, y, front in ((5, 5, False), (5, 10, True)):
        e.walk(x, y)
        e.frames(40)
        e.screenshot("house-in-front" if front else "house-walk-behind")
        state = e.state
        vx, vy = e.read(e.sym["visualx"], 4), e.read(e.sym["visualy"], 4)
        assert (vx, vy) == (x * 16, y * 16), "visual walking did not settle"
        camx, camy = e.read(e.sym["camx"], 4), e.read(e.sym["camy"], 4)
        step = e.read(e.sym["walkFrame"], 4) & 1
        sprite = people[(state.dir * 2 + step) * 384:(state.dir * 2 + step + 1) * 384]
        # mGBA can stop while the CPU is drawing the next software buffer.
        # Inspect the completed Mode 4 page currently displayed by the PPU.
        video = 0x0600a000 if e.read(0x04000000, 2) & 16 else 0x06000000
        compared = 0
        for row in range(24):
            for col in range(16):
                actor_color = sprite[row * 16 + col]
                ox, oy = vx + col - house.x, vy - 8 + row - house.y
                sx, sy = vx + col - camx, vy - 8 + row - camy
                if not (0 <= ox < width and 0 <= oy < height and
                        0 <= sx < 240 and 24 <= sy < 144):
                    continue
                house_color = art[oy * width + ox]
                if actor_color and house_color and actor_color != house_color:
                    expected = actor_color if front else house_color
                    actual = e.read(video + sy * 240 + sx)
                    assert actual == expected, ("incorrect visible layer", front,
                                                (sx, sy), expected, actual)
                    compared += 1
        assert compared >= 8, ("occlusion overlap proof was vacuous", front, compared)
        print("PASS: house", "behind player" if front else "occludes player",
              "at", compared, "overlapping opaque pixels")
    e.walk(9, 8)
    e.screenshot("town-house")
    e.walk(15, 14)


def old_save_expected(path):
    data = path.read_bytes()
    valid = []
    for base in (0, 512):
        raw = data[base:base + C.sizeof(p.Save)]
        state = p.Save.from_buffer_copy(raw)
        h = 2166136261
        for b in raw[:p.Save.checksum.offset]:
            h = ((h ^ b) * 16777619) & 0xffffffff
        if state.magic == 0x42424e44 and state.version == 1 and state.checksum == h:
            valid.append(state)
    assert valid, "baseline battery contains no valid save slot"
    return max(valid, key=lambda save: save.serial)


def backward_compatibility(baseline_dir):
    for filename in ("test-battery.sav", "test-backup.sav"):
        path = baseline_dir / filename
        expected = old_save_expected(path)
        e = VisualEmulator(path)
        try:
            assert e.mode == p.TITLE
            e.screenshot("old-save-title" if filename == "test-battery.sav" else "old-backup-title")
            if filename == "test-battery.sav":
                e.tap(p.DOWN)
                e.tap(p.A)
                assert e.mode == 11  # CONFIRM: cancel remains the safe default.
                e.screenshot("new-journey-confirmation")
                e.tap(p.A)
                assert e.mode == p.TITLE
                e.tap(p.UP)
            e.tap(p.DOWN)
            e.tap(p.A)
            assert e.mode == CONFIRM
            e.screenshot("new-journey-confirm")
            e.tap(p.B)
            e.tap(p.UP)
            e.tap(p.A)
            assert e.mode == p.FIELD
            assert bytes(e.state) == bytes(expected), ("old save changed on load", filename)
            e.screenshot("old-save-restored" if filename == "test-battery.sav" else "old-backup-restored")
            print("PASS: complete pre-overhaul", filename, "restored, including team/progress/inventory")
        finally:
            e.close()


def capture_screens():
    e = VisualEmulator()
    try:
        assert e.mode == p.TITLE
        e.screenshot("title")
        renderer_pace(e, "title")
        # The complete tutorial is driven naturally, and long dialogue is kept.
        e.tap(p.A)
        dialogue_pages(e, "intro")
        assert e.mode == p.FIELD
        e.screenshot("town-arrival")
        renderer_pace(e, "field")
        world = bytes(e.read(e.sym["world"] + i) for i in range(1536))
        assert digest(world) == REFERENCE["map_tiles"], "runtime collision map changed"
        occlusion_checks(e)
        e.walk(15, 11)
        e.tap(p.A)
        dialogue_pages(e, "mara")
        assert e.mode == p.CHOOSE
        for starter in range(3):
            e.screenshot("starter-" + str(starter))
            e.tap(p.RIGHT)
        e.tap(p.A)
        dialogue_pages(e, "first-partner")
        e.tap(p.START)
        e.screenshot("menu")
        e.tap(p.A)
        e.screenshot("team-one")
        e.tap(p.B)
        e.tap(p.DOWN)
        e.tap(p.A)
        assert e.mode == p.GUIDE
        e.screenshot("guide-known")
        e.tap(p.DOWN)
        e.screenshot("guide-unknown")
        e.tap(p.R)
        e.screenshot("guide-rules")
        e.tap(p.B)
        e.tap(p.DOWN)
        e.tap(p.A)
        e.screenshot("journal")
        e.tap(p.B)
        e.tap(p.B)
        p.recruit_first(e)
        p.garden(e)
        e.trail()
        e.walk(7, 13)
        e.tap(p.A)
        dialogue_pages(e, "camp")
        e.screenshot("trail-camp")
        e.tap(p.START)
        e.tap(p.A)
        e.screenshot("team-two")
        e.tap(p.SELECT)
        assert e.mode == p.GROWTH
        e.screenshot("growth-strider")
        e.tap(p.RIGHT)
        e.screenshot("growth-warden")
        e.tap(p.A)
        dialogue_pages(e, "growth-result")
        e.walk(26, 6)
        e.tap(p.A)
        dialogue_pages(e, "iris")
        e.screenshot("battle-log")
        e.logs()
        e.screenshot("battle-main")
        renderer_pace(e, "battle")
        e.tap(p.A)
        assert e.fight.menu == 1
        e.screenshot("battle-moves")
        e.tap(p.B)
        e.tap(p.DOWN)
        e.tap(p.A)
        assert e.fight.menu == 2
        e.screenshot("battle-swap")
        e.tap(p.B)
        e.tap(p.RIGHT)
        e.tap(p.DOWN)
        e.tap(p.A)
        assert e.fight.menu == 3
        e.screenshot("battle-pack")
        e.tap(p.B)
        assert e.fight.cursor == 0
        for turns in range(60):
            e.logs()
            if e.mode != p.BATTLE:
                break
            partner = e.state.friends[e.fight.active]
            if partner.hp < 13 and e.state.tonics:
                e.pick(0, menu=3)
            else:
                e.pick(1 if partner.energy >= 2 else 2, menu=0)
            e.logs()
        assert e.mode == p.ENDING and e.state.rival, "visual-route rival duel failed"
        e.screenshot("ending")
        e.tap(p.A)
        e.screenshot("postgame-lookout")
        assert e.lib.emu_audio_nonzero() > 0
        print("PASS: tutorial, recruitment, growth, garden, UI screens and rival via buttons")
    finally:
        e.close()


def contact_sheet():
    paths = VisualEmulator.captures
    columns, scale, label = 4, 2, 20
    rows = (len(paths) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * 240 * scale, rows * (160 * scale + label)), "#132239")
    draw = ImageDraw.Draw(sheet)
    for index, path in enumerate(paths):
        x = (index % columns) * 240 * scale
        y = (index // columns) * (160 * scale + label)
        sheet.paste(Image.open(path).resize((480, 320), Image.Resampling.NEAREST), (x, y))
        draw.text((x + 5, y + 323), path.stem.removeprefix("visual-"), fill="white")
    out = BUILD / "visual-contact-sheet.png"
    sheet.save(out)
    print("Saved", len(paths), "real-emulator screenshots and", out)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, default=None, help="archived pre-overhaul game.c")
    parser.add_argument("--old-saves", type=Path, default=Path("/workspace/visual-baseline"))
    parser.add_argument("--static-only", action="store_true")
    args = parser.parse_args()
    static_checks(args.baseline)
    if args.static_only:
        return
    p.build_bridge()
    p.assert_header()
    capture_screens()
    if all((args.old_saves / f).is_file() for f in ("test-battery.sav", "test-backup.sav")):
        backward_compatibility(args.old_saves)
    else:
        print("SKIP: old battery fixtures unavailable; pass --old-saves to check upgrade compatibility")
    contact_sheet()


if __name__ == "__main__":
    main()
