windres -J rc -O coff -i icons.rc -o icons.res
"i686-w64-mingw32-g++.exe" HL.c -mwindows icons.res wbemcli_i.obj -lole32 -loleaut32 -o HL.exe -static
