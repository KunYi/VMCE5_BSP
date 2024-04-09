@ rem filename: clean.bat
@ rem   - For use in CEBP 5.0 builds
@ rem   - Removes output file directories, which will be recreated during build
@ rem   - Also removes some individual file types
@ rem   - Must be run from platform\AladdinBSP directory
@
@echo off

del /s /q build.*
del /s /q *.bif
rd /s /q cesysgen\files
rd /s /q target
rd /s /q lib

for /r %%I in (.) do (
	if %%~nI == obj (
		@echo deleting %%~fI
		rd /s /q %%~fI
	)
)
