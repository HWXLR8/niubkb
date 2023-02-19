ergogen_bin=node_modules/ergogen/src/cli.js
output_dir=out
input=niubkb.yaml

.PHONY: all
all: $(input)
	$(ergogen_bin) $(input) -o $(output_dir)
