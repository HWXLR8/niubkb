ergogen_bin=node_modules/ergogen/src/cli.js
output_dir=out
input=niu.yaml

.PHONY: build
build: $(input)
	$(ergogen_bin) $(input) -o $(output_dir)

.PHONY: view
view: build
	pcbnew out/pcbs/niu.kicad_pcb
