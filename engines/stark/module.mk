MODULE := engines/stark

MODULE_OBJS := \
	console.o \
	detection.o \
	gfx/driver.o \
	gfx/opengls.o \
	gfx/openglsactor.o \
	gfx/openglsfade.o \
	gfx/openglsprop.o \
	gfx/openglssurface.o \
	gfx/opengltexture.o \
	gfx/renderentry.o \
	gfx/surfacerenderer.o \
	gfx/texture.o \
	formats/biff.o \
	formats/biffmesh.o \
	formats/iss.o \
	formats/tm.o \
	formats/xarc.o \
	formats/xmg.o \
	formats/xrc.o \
	model/animhandler.o \
	model/model.o \
	model/skeleton_anim.o \
	movement/followpath.o \
	movement/movement.o \
	movement/shortestpath.o \
	movement/stringpullingpath.o \
	movement/turn.o \
	movement/walk.o \
	resources/anim.o \
	resources/animhierarchy.o \
	resources/animscript.o \
	resources/animsoundtrigger.o \
	resources/bonesmesh.o \
	resources/bookmark.o \
	resources/camera.o \
	resources/container.o \
	resources/command.o \
	resources/dialog.o \
	resources/direction.o \
	resources/floor.o \
	resources/floorface.o \
	resources/floorfield.o \
	resources/fmv.o \
	resources/image.o \
	resources/item.o \
	resources/knowledge.o \
	resources/knowledgeset.o \
	resources/layer.o \
	resources/level.o \
	resources/light.o \
	resources/lipsync.o \
	resources/location.o \
	resources/object.o \
	resources/path.o \
	resources/pattable.o \
	resources/root.o \
	resources/script.o \
	resources/scroll.o \
	resources/sound.o \
	resources/speech.o \
	resources/string.o \
	resources/textureset.o \
	resourcereference.o \
	scene.o \
	services/archiveloader.o \
	services/dialogplayer.o \
	services/fontprovider.o \
	services/gameinterface.o \
	services/global.o \
	services/resourceprovider.o \
	services/services.o \
	services/stateprovider.o \
	services/staticprovider.o \
	services/userinterface.o \
	stark.o \
	tools/abstractsyntaxtree.o \
	tools/block.o \
	tools/command.o \
	tools/decompiler.o \
	ui/actionmenu.o \
	ui/button.o \
	ui/clicktext.o \
	ui/cursor.o \
	ui/dialogpanel.o \
	ui/fmvplayer.o \
	ui/gamewindow.o \
	ui/inventorywindow.o \
	ui/topmenu.o \
	ui/window.o \
	visual/actor.o \
	visual/image.o \
	visual/prop.o \
	visual/smacker.o \
	visual/text.o

# Include common rules
include $(srcdir)/rules.mk
