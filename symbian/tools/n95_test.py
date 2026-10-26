"""Remote test on the N95 over the USB cable: builds the .sisx (the game starts by itself when installed),
installs it with App TRK + runonphone and follows E:\\data\\claw\\openclaw.log through PC Suite (ConnAPI).

    python symbian/tools/n95_test.py              full package, install, follow the log for 90 s
    python symbian/tools/n95_test.py --quick      only the exe (partial upgrade)
    python symbian/tools/n95_test.py --log        just print the current log
    --autotest                                    straight into level 1 (no menu, no intro movies)
    --autoplay                                    with --autotest: Claw plays by himself (runs, jumps, attacks)
    --fire                                        with --autoplay: he shoots instead of using the sword
    --seconds N

Needs App TRK open on the phone and the n95.py / trk.py helpers (N95_TOOLS, default: the Daytona port's)."""
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', 'sis'))
TOOLS = os.environ.get('N95_TOOLS', os.path.normpath(os.path.join(
    HERE, '..', '..', '..', '..', 'conversiones', 'daytona_n95', 'symbian')))
sys.path.insert(0, TOOLS)

import make_sis  # noqa: E402
import n95  # noqa: E402
import trk  # noqa: E402

RUNONPHONE = r'C:\QtSDK\Symbian\tools\runonphone.exe'
LOG = 'E:\\data\\claw\\openclaw.log'
QUIT = 'E:\\data\\claw\\quit.txt'
AUTOPLAY = 'E:\\data\\claw\\autoplay.txt'


def install(sisx):
    port = trk.buscar_puerto()
    r = subprocess.run([RUNONPHONE, '-p', port, '-s', sisx], capture_output=True, text=True, errors='replace')
    if 'Installing finished' not in r.stdout:
        print(r.stdout[-2000:], r.stderr[-1000:])
        raise SystemExit('install failed (is App TRK open and connected?)')


def stop_running_game(phone):
    """A running game blocks the install: the game kills itself when quit.txt shows up in its data folder."""
    import tempfile
    with tempfile.TemporaryDirectory() as tmp:
        flag = os.path.join(tmp, 'quit.txt')
        open(flag, 'w').close()
        phone.subir(flag, 'E:\\data\\claw')
    time.sleep(3)
    if phone.existe(QUIT):
        phone.borrar(QUIT)  # (nothing was running: don't let it kill the new install)
    else:
        print('stopped the running game')


def read_log(phone):
    try:
        return n95.leer_log(phone, 0, LOG)
    except n95.ErrorN95:
        return None


def main(args):
    seconds = int(args[args.index('--seconds') + 1]) if '--seconds' in args else 90
    phone = n95.N95()
    try:
        if '--log' in args:
            for line in read_log(phone) or []:
                print(line)
            return
        autotest = '--autotest' in args
        sisx = make_sis.build(quick='--quick' in args or autotest, run=True)
        stop_running_game(phone)
        if phone.existe(LOG):
            try:
                phone.borrar(LOG)
            except n95.ErrorN95:
                pass  # (still open by a running game: the new run replaces it)
        if autotest:
            upload_config(phone, skip_menu=True)
            if '--autoplay' in args:
                upload_flag(phone, 'autoplay.txt', 'fire' if '--fire' in args else '')
        install(sisx)
        print('installed, running...')
        seen = 0
        end = time.time() + seconds
        while time.time() < end:
            time.sleep(5)
            lines = read_log(phone)
            if lines is None:
                continue
            for line in lines[seen:]:
                print('  ' + line[:200])
            seen = len(lines)
        if autotest:
            stop_running_game(phone)
            upload_config(phone, skip_menu=False)
            if phone.existe(AUTOPLAY):
                phone.borrar(AUTOPLAY)
    finally:
        phone.cerrar()


def upload_flag(phone, name, content=''):
    import tempfile
    with tempfile.TemporaryDirectory() as tmp:
        flag = os.path.join(tmp, name)
        with open(flag, 'w') as f:
            f.write(content)
        phone.subir(flag, 'E:\\data\\claw')


def upload_config(phone, skip_menu):
    """The package's config.xml, or a copy that starts straight in level 1 (for unattended measurements)."""
    import tempfile
    src = os.path.join(HERE, '..', 'data', 'claw', 'config.xml')
    text = open(src).read()
    if skip_menu:
        text = text.replace('<SkipMenu>false</SkipMenu>', '<SkipMenu>true</SkipMenu>')
        text = text.replace('<SkipMenuToLevel>13</SkipMenuToLevel>', '<SkipMenuToLevel>1</SkipMenuToLevel>')
        text = text.replace('</Assets>', '    <PlayIntroMovies>false</PlayIntroMovies>\n  </Assets>')
    with tempfile.TemporaryDirectory() as tmp:
        path = os.path.join(tmp, 'config.xml')
        with open(path, 'w', newline='\r\n') as f:
            f.write(text)
        phone.subir(path, 'E:\\data\\claw')


if __name__ == '__main__':
    main(sys.argv[1:])
