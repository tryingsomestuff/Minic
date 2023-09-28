import json

with open("/home/vivien/.config/cutechess/engines.json", "r") as f:
    all = json.load(f)
    for e in all:
        print(e['name'])
        if e['name'] == "minic_dev_linux_x64":
            with open("tuning.json", "r") as ff:
                tune = json.load(ff)
                for o in e['options']:
                    print(o['name'])
                    if o['name'] in tune['engines'][0]['fixed_parameters']:
                        print("found")
                        o['value'] = tune['engines'][0]['fixed_parameters'][o['name']]
    with open("/home/vivien/.config/cutechess/engines2.json", "w") as outfile:
        json.dump(all, outfile)