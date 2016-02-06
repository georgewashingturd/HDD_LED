windres -J rc -O coff -i icons.rc -o icons.res
g++ HL.c -mwindows icons.res -o HL.exe -static
