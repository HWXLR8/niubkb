ergogen_bin=ergogen/src/cli.js
output_dir=out

.PHONY: build
build: $(input)
	node $(ergogen_bin) . -o $(output_dir)

.PHONY: view
view: build
	pcbnew out/pcbs/niu.kicad_pcb
