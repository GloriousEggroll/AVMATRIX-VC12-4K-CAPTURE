# Builds the DKMS .deb packages with Docker (see doc/README.md).
UBUNTU_VERSIONS ?= 24.04 26.04
OUT ?= out

.PHONY: deb clean

deb: $(addprefix deb-,$(UBUNTU_VERSIONS))

deb-%:
	docker build -f packaging/Dockerfile --build-arg UBUNTU_VERSION=$* --output $(OUT) .

clean:
	rm -rf $(OUT)
