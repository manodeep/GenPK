#Change this to where you installed GadgetReader
GREAD=${CURDIR}/GadgetReader

#If Gadget was compiled with double precision output, you should define this flag
#to read it correctly
#OPT = -DDOUBLE_PRECISION_SNAP

#Check for a pkgconfig; if one exists we are probably debian.
ifeq ($(shell pkg-config --exists hdf5-serial && echo 1),1)
	HDF_LINK = $(shell pkg-config --libs hdf5-serial) -lhdf5_hl
	HDF_INC = $(shell pkg-config --cflags hdf5-serial)
else
	HDF_INC = $(HDF5_BASE)/include
	HDF_LINK = -L${HDF5_BASE}/lib -lhdf5 -lhdf5_hl -Xlinker -rpath -Xlinker ${HDF5_BASE}/lib
endif

LFLAGS += -lfftw3_threads -lfftw3 -lpthread -lrgad -L${GREAD} -Wl,-rpath,$(GREAD) $(HDF_LINK) -Lbigfile/src -lbigfile

#Mac's ld doesn't support --no-add-needed, so check for it.
#We are looking for the response: ld unknown option: --no-add-needed
LDCHECK:=$(shell ld --as-needed 2>&1)
ifneq (unknown,$(findstring unknown,${LDCHECK}))
  LFLAGS +=-Wl,--no-add-needed,--as-needed
endif
#Are we using gcc or icc?
ifeq (icc,$(findstring icc,${CC}))
  CFLAGS += -g -c -w1 -qopenmp $(OPT) -I${GREAD} -axCORE-AVX2 -O2
  LINK +=${CXX} -qopenmp
else
  CFLAGS +=-O2 -ffast-math -g -c -Wall $(OPT) -I${GREAD} $(HDF_INC) -mavx2 -march=broadwell
  LINK +=${CXX} $(PRO)
  LFLAGS += -lm
GCCV:=$(shell gcc --version )
ifneq (darwin,$(findstring darwin,${GCCV}))
  LFLAGS += -lgomp
  LINK += -fopenmp
  CFLAGS += -fopenmp
endif
endif
PRO=#-pg
#gcc
PPFLAGS:=$(CFLAGS)
CXXFLAGS+= $(PPFLAGS)
objs = powerspectrum.o fieldize.o read_fieldize.o utils.o read_fieldize_bigfile.o
.PHONY:all love clean test dist

all: librgad.so gen-pk

gen-pk: gen-pk.o ${objs}
	${LINK} $^ ${LFLAGS} -o $@

$(GREAD)/Makefile:
	git submodule init
	git submodule update

librgad.so: $(GREAD)/Makefile
	cd $(GREAD); VPATH=$(GREAD) make $@

read_fieldize_bigfile.o: bigfile/src/libbigfile.a

bigfile/src/libbigfile.a:
	cd bigfile/src; VPATH=bigfile/src MPICC=$(CC) make libbigfile.a

powerspectrum.o: powerspectrum.c
	$(CC) -std=gnu99 $(CFLAGS) $^
%.o: %.cpp gen-pk.h

btest: test.cpp ${objs}
	${LINK} $(OPT) -I${GREAD} $^ ${LFLAGS} -lboost_unit_test_framework -o $@

test: btest librgad.so
	./$<

dist: Makefile README $(head) Doxyfile gen-pk.cpp  read_fieldize.cpp  test.cpp  utils.cpp gen-pk.h fieldize.cpp powerspectrum.c test_g2_snap.0 test_g2_snap.1
	tar -czf genpk.tar.gz $^

doc: Doxyfile gen-pk.h
	doxygen $<

.PHONY: clean celna clena celan

celna clena celan: clean

clean:
	-rm -f ${objs} gen-pk.o gen-pk
