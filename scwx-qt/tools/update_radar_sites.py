#!/usr/bin/env python3
import requests
import json
import argparse
import csv

NOAA_BASE = "https://www.ncdc.noaa.gov/homr/services/station"
WARNING = "\033[93mWARNING: Updating radar sites may break tests in \
'test/source/scwx/qt/config/radar_site.test.cpp'\033[39m"

# Get the noaa station data.
# platform is what platform should be searched for
# current (should) filter to only current stations (always is filtered)
# icao is a ICAO identifier. Without it we search for all.
def get_noaa_stations(platform, current, icao = None):
    params = {
        "definitions":  "false",
        "phrData":      "false",
    }
    if platform is not None:
        params["platform"] = platform
    if current:
        params["current"] = "true"
    if icao is not None:
        params["qid"] = f"ICAO:{icao}"

    res = requests.get(NOAA_BASE + "/search", params = params)

    if res.ok:
        return res.json()["stationCollection"]["stations"]
    else:
        print("NETWORK ERROR: Could not get resources from NOAA HOMR")
        print(res.text)
        exit(5)

# dictionary to convert NOAA types to Supercell_wx types
NOAA_TYPE_DICT = {
    "TDWR":     "tdwr",
    "NEXRAD":  "wsr88d"
}

# Given an list of objects, find the object with the best value for key.
# The values that appear earlier in values are better.
# subKey will take return the value under that key, not the full object.
# parser (needs subKey) is a function that will have the value found by
#   subKey and return a parsed version of it (often 'float' because HOMR
#   data uses strings for floats)
def extract_best(items, key, values, subKey = None, parser = None):
    valuesPart = enumerate(reversed(values))
    valueDict = dict([(k,v) for v,k in valuesPart])
    best = None
    bestInd = -1

    for item in items:
        index = valueDict.get(item[key], -1)
        if bestInd is None or bestInd < index:
            bestInd = index
            best = item


    if subKey is None or best is None:
        return best

    if parser is not None:
        return parser(best[subKey])

    return best[subKey]

# Turn the noaa stations into a dictionary that can be used to update the locations and elevations.
def make_noaa_stations_dict(noaaStations):
    output = {}

    for station in noaaStations:
        stationId = extract_best(station["identifiers"], "idType", ["NEXRAD", "ICAO"], "id")

        if stationId in output: # some stations are repeaded in non NEXRAD/TDWR locations.
            continue

        stationDict = {}
        stationDict["lat"]   = float(station["header"]["latitude_dec"])
        stationDict["lon"]   = float(station["header"]["longitude_dec"])
        stationDict["elevation"] = extract_best(station["location"].get("elevations", []),
                                                "elevationType",
                                                ["GROUND"],
                                                "elevationFeet",
                                                float)
        # These are some things that could be updated from the NOAA HOMR data,
        # but are not necessary in the same format, so they are disabled.
        """
        stationDict["id"] = stationId
        stationDict["country"] = station["location"]["geoInfo"]["countries"][0]["country"]
        if "stateProvinces" in station["location"]["geoInfo"]:
            stationDict["state"]   = station["location"]["geoInfo"]["stateProvinces"][0]["stateProvince"]
        else:
            stationDict["state"] = None
        stationDict["place"]   = extract_best(station["names"], "nameType", ["PRINCIPAL"], "name")
        stationDict["type"]    = NOAA_TYPE_DICT[station["platforms"][0]["platform"]]
        #stationDict["tz"]      = station["location"]["geoInfo"]["utcOffsets"][0]["utcOffset"] # This is UTC offset, not timezone
        """

        output[stationId] = stationDict

    return output

# Get the list of updated stations (not in place), using the noaaStationsDict
# from make_noaa_stations_dict
def update_stations(noaaStationsDict, previousStations, toUpdate = None):
    newStations = []
    for station in previousStations:
        newStation = station.copy()

        if not station["id"] in noaaStationsDict:
            # may be good idea to add fallback to a ICAO search for non active
            # stations.
            print(f"WARNING: Station '{station['id']}' not found in noaa data")

            if "elevation" not in station:
                newStation["elevation"] = None
        elif toUpdate is None or station["id"] in toUpdate:
            newStation.update(noaaStationsDict[station["id"]])
        else:
            newStation["elevation"] = noaaStationsDict[station["id"]]["elevation"]

        newStations.append(newStation)

    return newStations

# Read in csv file describing which locations to update.
# Elevation data is always updated.
def get_to_update_file(filename):
    with open(filename) as file:
        r = csv.reader(file)
        next(r)
        toUpdate = set()
        for row in r:
            if len(row) == 2 and row[1] == "HOMR":
                toUpdate.add(row[0])
    return toUpdate


# Customized dump routine. Formats it as one station per row, aligning items.
def custom_dump(stations, file):
    file.write("[\n")
    lengths = {}
    lastKey = None
    keys = None

    # Find length for each value, and ensure all stations have the same keys.
    for station in stations:
        for key, value in station.items():
            length = len(json.dumps(value))
            if key in lengths:
                lengths[key] = max(length, lengths[key])
            else:
                lengths[key] = length
            lastKey = key

        newKeys = list(station.keys())
        if keys is None:
            keys = newKeys
        elif keys != newKeys:
            print("DUMP ERROR: Stations did not have the same keys.")
            exit(3)

    # Write out each station with the correct format.
    lastType = None
    for station in stations:
        # put an empty line between NEXRAD and TDWR.
        if lastType is not None and lastType != station["type"]:
            file.write("\n")

        file.write("\t{ ")

        for key, value in station.items():
            value = json.dumps(value)
            file.write(f'"{key}": {value}')

            if key != lastKey:
                file.write(", ")
                file.write(" " * (lengths[key] - len(value)))

        if station == stations[-1]:
            file.write(" }\n")
        else:
            file.write(" },\n")

        lastType = station["type"]

    file.write("]\n")

# Write coordinates out to a file. Useful for checking against map program.
def make_coords(stations, file):
    for station in stations:
        lat = str(abs(station["lat"]))
        lat += "N" if station["lat"] > 0 else "S"

        lon = str(abs(station["lon"]))
        lon += "E" if station["lon"] > 0 else "W"

        file.write(f"{lat} {lon}\n")

def main():
    parser = argparse.ArgumentParser(
            description="""Update supercell-wx's location data for towers form NOAA's HOMR database.\n
            Recommended Arguments: -u ../res/config/radar_sites.json -t -w""")
    parser.add_argument("--current_file", "-u", type = str, default = None, required = False,
                        help = "The 'radar_sites.json' file to update. Without this option, this will generate a new file")
    parser.add_argument("--test_updated", "-t", default = False, action = "store_true",
                        help = "Read in the updated file to ensure it is valid JSON. Should be used.")
    parser.add_argument("--to_update_csv", "-U", type = str, default = None, required = False,
                        help = "Choose a CSV describing which stations to update. \
First column is station ID, Second is HOMR if it should be updated. First row is a header.")
    parser.add_argument("--updated_file", "-o", type = str, default = None, required = False,
                        help = "The updated 'radar_sites.json' file. The default is to overwrite the current one.")
    parser.add_argument("--coord_file", "-c", type = str, default = None, required = False,
                        help = "Output an additional file with the coordinates of each site.")
    parser.add_argument("--resp_file", "-r", type = str, default = None, required = False,
                        help = "Output most of the JSON from the responses.")
    parser.add_argument("--input_json", "-i", type = str, default = None, required = False,
                        help = "Instead of querying NOAA, just read in a JSON file made by \"-r\".")
    parser.add_argument("--json_dump", "-j", default = False, action = "store_true",
                        help = "Uses 'json.dump' instead of the custom dump function. Has worse formatting.")
    parser.add_argument("--more_radars", "-m", default = False, action = "store_true",
                        help = "Get AWOS and UPPERAIR stations as well. Should NOT be used.")
    parser.add_argument("--current_only", "-C", default = False, action = "store_true",
                        help = "Get only currently active stations. Does not seem to change anything.")
    parser.add_argument("--warn", "-w", default = False, action = "store_true",
                        help = "Display a warning about breaking a test by updating the radar sites.")

    args = parser.parse_args()
    # default to updating the same file as input
    if args.updated_file is None:
        if args.current_file is None:
            parser.error("Needs 'current_file' or 'updated_file'")
        args.updated_file = args.current_file

    previousStations = None
    if args.current_file is not None:
        print(f"Reading Current Sites from '{args.current_file}'")
        with open(args.current_file, "r") as file:
            previousStations = json.load(file)

    toUpdate = None
    if args.to_update_csv is not None:
        toUpdate = get_to_update_file(args.to_update_csv)

    if args.input_json is None:
        print("Getting NEXRAD stations")
        noaaStations = get_noaa_stations("NEXRAD", args.current_only)

        print("Getting TDWR stations")
        noaaStations += get_noaa_stations("TDWR", args.current_only)

        if args.more_radars: # Should not be used
            print("Getting AWOS stations")
            noaaStations += get_noaa_stations("AWOS", args.current_only)

            print("Getting UPPERAIR stations")
            noaaStations += get_noaa_stations("UPPERAIR", args.current_only)
    else:
        with open(args.input_json, "r") as file:
            noaaStations = json.load(file)

    if args.resp_file is not None:
        with open(args.resp_file, "w") as file:
            json.dump(noaaStations, file, indent=4)

    print("Processing Data")
    noaaStationsDict = make_noaa_stations_dict(noaaStations)

    if args.current_file is None:
        newStations = list(noaaStationsDict.values())
    else:
        newStations = update_stations(noaaStationsDict, previousStations, toUpdate)

    print(f"Saving Updated Sites to '{args.updated_file}'")
    with open(args.updated_file, "w") as file:
        if args.json_dump:
            json.dump(newStation, file)
        else:
            custom_dump(newStations, file)

    if args.coord_file is not None:
        print(f"Saving Coordinates to '{args.coord_file}'")
        with open(args.coord_file, "w") as file:
            make_coords(newStations, file)

    if args.test_updated:
        failed = False
        with open(args.updated_file, "r") as file:
            try:
                data = json.load(file)
                if len(data) < len(newStations):
                    print(f"TEST ERROR: Only read in {len(data)} out of {len(newStations)} items.")
                    failed = True
                if json.dumps(data) != json.dumps(newStations):
                    print(f"TEST ERROR: Dumps are not equal")
                    failed = True
            except Exception as e:
                print(e)
                failed = True

            if failed:
                exit(4)

    if args.warn:
        print(WARNING)

if __name__ == "__main__":
    main()


