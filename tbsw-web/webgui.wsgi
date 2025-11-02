import sys
sys.path.append('/home/tbuser/koodit/tbsw-web/venv/lib/python3.5/site-packages')

import imp
webgui = imp.load_source("wsguiserv", "/home/tbuser/koodit/tbsw-web/wsguiserv.py")
application = webgui.app

