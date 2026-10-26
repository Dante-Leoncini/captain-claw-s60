# Application icon: symbian/gfx/openclaw_icon.svg -> \resource\apps\OpenClaw_icon.mif

ifeq (WINS,$(findstring WINS, $(PLATFORM)))
ZDIR=$(EPOCROOT)epoc32\release\$(PLATFORM)\$(CFG)\Z
else
ZDIR=$(EPOCROOT)epoc32\data\z
endif

TARGETDIR=$(ZDIR)\resource\apps
ICONTARGETFILENAME=$(TARGETDIR)\OpenClaw_icon.mif

do_nothing :
	@rem do_nothing

MAKMAKE : do_nothing

BLD : do_nothing

CLEAN : do_nothing

LIB : do_nothing

CLEANLIB : do_nothing

# /V3 keeps the classic SVGB: the Symbian^3 mifconv would emit NVG, which S60 3rd can't show
RESOURCE :
	mifconv $(ICONTARGETFILENAME) /V3 /c32 ..\gfx\openclaw_icon.svg

FREEZE : do_nothing

SAVESPACE : do_nothing

RELEASABLES :
	@echo $(ICONTARGETFILENAME)

FINAL : do_nothing
