# $Id: Makefile,v 1.12 2019-02-21 18:18:42-08 - - $

MKFILE      = Makefile
DEPFILE     = ${MKFILE}.dep
NOINCL      = ci clean spotless
NEEDINCL    = ${filter ${NOINCL}, ${MAKECMDGOALS}}
GMAKE       = ${MAKE} --no-print-directory

GPPWARN     = -Wall -Wextra -Werror -Wpedantic -Wshadow -Wold-style-cast
GPPOPTS     = ${GPPWARN} -fdiagnostics-color=never
COMPILECPP  = g++ -std=gnu++17 -g -O0 ${GPPOPTS}
MAKEDEPCPP  = g++ -std=gnu++17 -MM ${GPPOPTS}
UTILBIN     = /afs/cats.ucsc.edu/courses/cmps109-wm/bin

MODULES     = logstream protocol sockets
EXECBINS    = cix cixd
ALLMODS     = ${MODULES} ${EXECBINS}
SOURCELIST  = ${foreach MOD, ${ALLMODS}, ${MOD}.h ${MOD}.tcc ${MOD}.cpp}
ALLSOURCE   = ${wildcard ${SOURCELIST}} ${MKFILE}
CPPLIBS     = ${wildcard ${MODULES:=.cpp}}
OBJLIBS     = ${CPPLIBS:.cpp=.o}
CIXOBJS     = cix.o ${OBJLIBS}
CIXDOBJS    = cixd.o ${OBJLIBS}
CLEANOBJS   = ${OBJLIBS} ${CIXOBJS} ${CIXDOBJS}
LISTING     = Listing.ps

all: ${DEPFILE} ${EXECBINS}

cix: ${CIXOBJS}
	${COMPILECPP} -o $@ ${CIXOBJS}

cixd: ${CIXDOBJS}
	${COMPILECPP} -o $@ ${CIXDOBJS}

%.o: %.cpp
	- ${UTILBIN}/checksource $<
	- ${UTILBIN}/cpplint.py.perl $<
	${COMPILECPP} -c $<

ci: ${ALLSOURCE}
	${UTILBIN}/cid + ${ALLSOURCE}
	- ${UTILBIN}/checksource ${ALLSOURCE}

lis: all ${ALLSOURCE} ${DEPFILE}
	- pkill gv
	${UTILBIN}/mkpspdf ${LISTING} ${ALLSOURCE} ${DEPFILE}

clean:
	- rm ${LISTING} ${LISTING:.ps=.pdf} ${CLEANOBJS} core

spotless: clean
	- rm ${EXECBINS} ${DEPFILE}


dep: ${ALLCPPSRC}
	@ echo "# ${DEPFILE} created `LC_TIME=C date`" >${DEPFILE}
	${MAKEDEPCPP} ${ALLSOURCE} >>${DEPFILE}

${DEPFILE}:
	@ touch ${DEPFILE}
	${GMAKE} dep

again: ${ALLSOURCE}
	${GMAKE} spotless dep ci all lis

ifeq (${NEEDINCL}, )
include ${DEPFILE}
endif

