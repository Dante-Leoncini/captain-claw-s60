"""Builds the ASSETS.ZIP shipped with the S60 port: OpenClaw's Build_Release/ASSETS.ZIP with the
"Replay Movies" menu enabled (a page listing the ten movies of CLAW.FEC).

    python symbian/tools/make_assets.py [output]     (default: build_sis/ASSETS.ZIP)"""
import os
import sys
import zipfile

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
SOURCE = os.path.join(ROOT, 'Build_Release', 'ASSETS.ZIP')
IMAGES = '/STATES/MENU/IMAGES/MAIN/REPLAYMOVIES/'
NAMES = ('ONE', 'TWO', 'THREE', 'FOUR', 'FIVE', 'SIX', 'SEVEN', 'EIGHT', 'NINE', 'TEN')


def item(name, kind, state, images, y, event, hotkey=None):
    lines = ['        <MenuItem>',
             '            <Name>%s</Name>' % name,
             '            <Type>%s</Type>' % kind]
    if hotkey:
        lines.append('            <Hotkey>%s</Hotkey>' % hotkey)
    lines.append('            <State>%s</State>' % state)
    for tag, image in zip(('InactiveImage', 'ActiveImage'), images):
        lines.append('            <%s>%s%s</%s>' % (tag, IMAGES, image, tag))
    # (x: the menu centers each text by its real width)
    lines.append('            <Position x="220" y="%d" />' % y)
    if event:
        lines.append('            <GeneratedEvent>')
        lines += ['                ' + line for line in event]
        lines.append('            </GeneratedEvent>')
    lines.append('        </MenuItem>')
    return lines


def replay_movies_page():
    back = ['<Type>SwitchPage</Type>', '<PageName>MenuPage_Main</PageName>']
    lines = ['',
             '    <!-- REPLAY MOVIES PAGE -->',
             '    <MenuPage>',
             '        <PageName>MenuPage_ReplayMovies</PageName>',
             '        <KeyboardEvent>',
             '            <KeyType>Escape</KeyType>',
             '            <GeneratedEvent>',
             '                <Type>SwitchPage</Type>',
             '                <PageName>MenuPage_Main</PageName>',
             '            </GeneratedEvent>',
             '        </KeyboardEvent>']
    lines += item('REPLAY_MOVIES_TEXT', 'Text', 'Inactive', ['033_TITLE.PCX'], 160, None)
    for i, name in enumerate(NAMES):
        first = i * 3 + 1
        images = ['%03d_MOVIE%s.PCX' % (first, name), '%03d_MOVIE%s.PCX' % (first + 1, name)]
        # (the first entry is the montage at the end of CLAW.FEC, the others follow the story order)
        movie = 10 if i == 0 else i
        event = ['<Type>PlayMovie</Type>', '<MovieIndex>%d</MovieIndex>' % movie]
        lines += item('MOVIE_%s_BUTTON' % name, 'Button', 'Active' if i == 0 else 'Inactive', images,
                      200 + i * 22, event)
    lines += item('BACK_BUTTON', 'Button', 'Inactive', ['031_BACK.PCX', '032_BACK.PCX'], 425, back)
    lines.append('    </MenuPage>')
    return '\r\n'.join(lines) + '\r\n'


def patch_menu(xml):
    button = xml.index('<Name>REPLAY_MOVIES_BUTTON</Name>')
    state = xml.index('<State>Disabled</State>', button)
    assert state - button < 200
    xml = xml[:state] + '<State>Inactive</State>' + xml[state + len('<State>Disabled</State>'):]
    end = xml.rindex('</GameMenu>')
    return xml[:end] + replay_movies_page() + xml[end:]


def main(args):
    out = args[0] if args else os.path.join(ROOT, 'build_sis', 'ASSETS.ZIP')
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with zipfile.ZipFile(SOURCE) as src, zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as dst:
        for info in src.infolist():
            data = src.read(info.filename)
            if info.filename == 'MENU.xml':
                data = patch_menu(data.decode('latin1')).encode('latin1')
            dst.writestr(info, data, compress_type=zipfile.ZIP_DEFLATED)
    print(out)
    return out


if __name__ == '__main__':
    main(sys.argv[1:])
