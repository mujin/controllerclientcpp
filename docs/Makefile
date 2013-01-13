openrave_version=$(shell openrave-config --version)
doxycommands="STRIP_FROM_PATH = $(PWD)\\n\
PROJECT_NUMBER = $(openrave_version)\\n\
ALIASES += openraveversion=`openrave-config --version`\\n\
"

.PHONY: html_% html clean clean_doxygen

# this empty line prevents from any intermediate files from getting deleted.... weird
.SECONDARY:

all: html_en

build/%/coreapihtml/index.html: Doxyfile.html Doxyfile.% ../include/mujincontrollerclient/*.h
	rm -f doxygenhtml_installed_$*
	mkdir -p build
	echo "$(doxycommands)" | cat Doxyfile.html Doxyfile.$* - > build/Doxyfile.html.$*
	doxygen build/Doxyfile.html.$*
	touch doxygenhtml_installed_$*

doxygenhtml_installed_%: build/%/coreapihtml/index.html ;

html_%: doxygenhtml_installed_% ;
htmlall: html_en html_ja ;

#sendtoserver: jsonall htmlall
#	tar cjf openravedocs.tgz build/$(language)/coreapihtml build/$(language)/sphinxjson
#	scp openravedocs.tgz $server:$targetdir
#	ssh $server "cd $targetdir; rm -rf build; tar xjf openravedocs.tgz; mv -f en en_old; mv -f ja ja_old; mv build/en .; mv build/ja .; rm -rf en_old ja_old build"
#scp openravedocs.tgz diankov@programmingvision.com:"~/openrave/"
#ssh diankov@programmingvision.com "cd ~/openrave; rm -rf build; tar xjf openravedocs.tgz; mv -f en en_old; mv -f ja ja_old; mv build/en .; mv build/ja .; rm -rf en_old ja_old build"

clean_doxygen_%:
	rm -rf build/Doxyfile.*.$* build/$*

clean:
	rm -rf build *_installed_*
