; встроенные ресурсы
section .rodata

global file_icon_bmp_start
global file_icon_bmp_end

file_icon_bmp_start:
    incbin "src/gui/apps/file/file_icon.bmp"
file_icon_bmp_end:

global music_icon_bmp_start
global music_icon_bmp_end

music_icon_bmp_start:
    incbin "src/gui/apps/music/music_icon.bmp"
music_icon_bmp_end:

global about_bmp_start
global about_bmp_end

about_bmp_start:
    incbin "src/gui/apps/about/about.bmp"
about_bmp_end:

global wallpaper_bmp_start
global wallpaper_bmp_end

wallpaper_bmp_start:
    incbin "src/gui/wallpaper/wallpaper.bmp"
wallpaper_bmp_end:

global cursor_bmp_start
global cursor_bmp_end

cursor_bmp_start:
    incbin "src/gui/cursor/cursor.bmp"
cursor_bmp_end:

global volume_bmp_start
global volume_bmp_end

volume_bmp_start:
    incbin "src/gui/volume.bmp"
volume_bmp_end:

global volume_mute_bmp_start
global volume_mute_bmp_end

volume_mute_bmp_start:
    incbin "src/gui/volume_mute.bmp"
volume_mute_bmp_end:

global volume_min_bmp_start
global volume_min_bmp_end

volume_min_bmp_start:
    incbin "src/gui/volume_min.bmp"
volume_min_bmp_end:

global volume_max_bmp_start
global volume_max_bmp_end

volume_max_bmp_start:
    incbin "src/gui/volume_max.bmp"
volume_max_bmp_end:

global calc_icon_bmp_start
global calc_icon_bmp_end

calc_icon_bmp_start:
    incbin "src/gui/apps/calc/calc_icon.bmp"
calc_icon_bmp_end:

global notes_icon_bmp_start
global notes_icon_bmp_end

notes_icon_bmp_start:
    incbin "src/gui/apps/notes/notes_icon.bmp"
notes_icon_bmp_end:

global settings_icon_bmp_start
global settings_icon_bmp_end

settings_icon_bmp_start:
    incbin "src/gui/apps/settings/settings_icon.bmp"
settings_icon_bmp_end:

global terminal_icon_bmp_start
global terminal_icon_bmp_end

terminal_icon_bmp_start:
    incbin "src/gui/apps/file/terminal_icon.bmp"
terminal_icon_bmp_end:

global wallpaper_day_bmp_start
global wallpaper_day_bmp_end

wallpaper_day_bmp_start:
    incbin "src/gui/wallpaper/wallpaper_day.bmp"
wallpaper_day_bmp_end:

global wallpaper_night_bmp_start
global wallpaper_night_bmp_end

wallpaper_night_bmp_start:
    incbin "src/gui/wallpaper/wallpaper_night.bmp"
wallpaper_night_bmp_end:

global doom_icon_bmp_start
global doom_icon_bmp_end

doom_icon_bmp_start:
    incbin "src/gui/apps/doom/doom.bmp"
doom_icon_bmp_end:

global doom_wad_start
global doom_wad_end

doom_wad_start:
    incbin "src/gui/apps/doom/DOOM.WAD"
doom_wad_end:

global notepad_elf_start
global notepad_elf_end

notepad_elf_start:
    incbin "build/apps/notepad.elf"
notepad_elf_end:

global explorer_elf_start
global explorer_elf_end

explorer_elf_start:
    incbin "build/apps/explorer.elf"
explorer_elf_end:

global notepad_kea_start
global notepad_kea_end
notepad_kea_start:
    incbin "build/apps/notepad.kea"
notepad_kea_end:

global explorer_kea_start
global explorer_kea_end
explorer_kea_start:
    incbin "build/apps/explorer.kea"
explorer_kea_end:

global taskmgr_kea_start
global taskmgr_kea_end
taskmgr_kea_start:
    incbin "build/apps/taskmgr.kea"
taskmgr_kea_end:

global paint_kea_start
global paint_kea_end
paint_kea_start:
    incbin "build/apps/paint.kea"
paint_kea_end:

global shell_kea_start
global shell_kea_end
shell_kea_start:
    incbin "build/apps/shell.kea"
shell_kea_end:

global settings_elf_start
global settings_elf_end
settings_elf_start:
    incbin "build/apps/settings.elf"
settings_elf_end:

global settings_kea_start
global settings_kea_end
settings_kea_start:
    incbin "build/apps/settings.kea"
settings_kea_end:

global about_elf_start
global about_elf_end
about_elf_start:
    incbin "build/apps/about.elf"
about_elf_end:

global about_kea_start
global about_kea_end
about_kea_start:
    incbin "build/apps/about.kea"
about_kea_end:


