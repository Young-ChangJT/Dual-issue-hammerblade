u7k1-link   := https://drive.google.com/uc?id=1hefg63DFUdCSs-dE8vOgX-0komVS_nXS
g7k1-link   := https://drive.google.com/uc?id=1vFrohxOijg-HjckN0PTN22KGXlVJTtis
u10k8-link  := https://drive.google.com/uc?id=127ZnCdQg4xmgJtrck9uzj5xmQcwDJNx6
g10k8-link  := https://drive.google.com/uc?id=1iu3uKe-QGHLgHt-PbYfKxQtDjaFidVZk
u16k16-link := https://drive.google.com/uc?id=10klumtpAqpSRJD43tc3eDivyTw9SlAdm
g16k16-link := https://drive.google.com/uc?id=1RhNjC2m_m2OEExF7j2BN0YhHJZfJ3U9W


INPUTS = 	u7k1.mtx	\
					g7k1.mtx 	\
					u10k8.mtx	\
					g10k8.mtx	\
					u16k16.mtx	\
					g16k16.mtx	
				

all: $(INPUTS)

%.mtx:
	gdown $($*-link)
	tar zxf $*.mtx.tar.gz

.PHONY: clean
clean:
	rm *.mtx *.tag.gz
