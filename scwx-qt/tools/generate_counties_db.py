import argparse
import geopandas as gpd
import pathlib
import sqlite3

class DatabaseInfo:
    def __init__(self):
        self.sqlConnection_ = None
        self.sqlCursor_     = None

def ParseArguments():
    parser = argparse.ArgumentParser(description='Generate counties SQLite database')
    parser.add_argument("-c", "--county_dbf",
                        metavar = "filename",
                        help    = "input county database",
                        dest    = "inputCountyDbs_",
                        action  = "extend",
                        nargs   = "+",
                        default = [],
                        type    = pathlib.Path)
    parser.add_argument("-z", "--zone_dbf",
                        metavar = "filename",
                        help    = "input zone database",
                        dest    = "inputZoneDbs_",
                        action  = "extend",
                        nargs   = "+",
                        default = [],
                        type    = pathlib.Path)
    parser.add_argument("-s", "--state_dbf",
                        metavar = "filename",
                        help    = "input state database",
                        dest    = "inputStateDbs_",
                        action  = "extend",
                        nargs   = "+",
                        default = [],
                        type    = pathlib.Path)
    parser.add_argument("-o", "--output_db",
                        metavar  = "filename",
                        help     = "output sqlite database",
                        dest     = "outputDb_",
                        type     = pathlib.Path,
                        required = True)
    return parser.parse_args()

def Prepare(dbInfo, outputDb):
    # Truncate existing database
    file = open(outputDb, 'w')
    file.close()

    # Establish SQLite database connection
    dbInfo.sqlConnection_ = sqlite3.connect(outputDb)
    
    # Set row factory for name-based access to columns
    dbInfo.sqlConnection_.row_factory = sqlite3.Row

    dbInfo.sqlCursor_     = dbInfo.sqlConnection_.cursor()

    # Create database tables
    dbInfo.sqlCursor_.execute("""CREATE TABLE counties(
        id   TEXT NOT NULL PRIMARY KEY,
        name TEXT)""")
    dbInfo.sqlCursor_.execute("""CREATE TABLE states(
        state TEXT NOT NULL PRIMARY KEY,
        name  TEXT NOT NULL)""")

def ProcessCountiesDbf(dbInfo, dbfFilename):
    # County area type
    areaType = 'C'

    # Read dataframe
    dbfTable = gpd.read_file(filename        = dbfFilename,
                             include_fields  = ["STATE", "FIPS", "COUNTYNAME"],
                             ignore_geometry = True)
    dbfTable.drop_duplicates(inplace=True)

    for row in dbfTable.itertuples():
        # Generate a FIPS ID compatible with UGC format (NWSI 10-1702)
        fipsId = "{}{}{:03}".format(row.STATE, areaType, (int(row.FIPS) % 1000))

        # Insert FIPS ID and name pair into database
        try:
            dbInfo.sqlCursor_.execute("INSERT INTO counties VALUES (?, ?)", (fipsId, row.COUNTYNAME))
        except:
            print("Skipping duplicate county:", fipsId, row.COUNTYNAME)

def ProcessStateDbf(dbInfo, dbfFilename):
    print("Processing states and territories file:", dbfFilename)

    # Read dataframe
    dbfTable = gpd.read_file(filename        = dbfFilename,
                             include_fields  = ["STATE", "NAME"],
                             ignore_geometry = True)
    dbfTable.drop_duplicates(inplace=True)

    for row in dbfTable.itertuples():
        # Insert data into database
        try:
            dbInfo.sqlCursor_.execute("INSERT INTO states VALUES (?, ?)", (row.STATE, row.NAME))
        except:
            print("Error inserting row:", row.STATE, row.NAME)

def ProcessZoneDbf(dbInfo, dbfFilename):
    print("Processing zone file:", dbfFilename)
    # Zone area type
    areaType = 'Z'

    # Read dataframe
    dbfTable = gpd.read_file(filename        = dbfFilename,
                             include_fields  = ["ID", "STATE", "ZONE", "NAME"],
                             ignore_geometry = True)
    dbfTable.drop_duplicates(inplace=True)

    for row in dbfTable.itertuples():
        # Generate a FIPS ID compatible with UGC format (NWSI 10-1702)
        if "ID" in dbfTable.columns:
            fipsId = row.ID
        else:
            fipsId = "{}{}{:03}".format(row.STATE, areaType, (int(row.ZONE) % 1000))

        # Insert FIPS ID and name pair into database
        try:
            dbInfo.sqlCursor_.execute("INSERT INTO counties VALUES (?, ?)",
                                      (fipsId, row.NAME))
        except:
            # Only print warning if FIPS ID has multiple names
            result = dbInfo.sqlCursor_.execute("SELECT name FROM counties WHERE id = :fipsId",
                                               {"fipsId": fipsId})
            resultRow = result.fetchone()
            if resultRow is not None:
                if resultRow["name"] != row.NAME:
                    print("Skipping duplicate zone:", fipsId, row.NAME)

def PostProcess(dbInfo):
    # Commit changes and close database
    dbInfo.sqlConnection_.commit()
    dbInfo.sqlConnection_.close()

dbInfo = DatabaseInfo()
args   = ParseArguments()
Prepare(dbInfo, args.outputDb_)

for countyDb in args.inputCountyDbs_:
    ProcessCountiesDbf(dbInfo, countyDb)

for zoneDb in args.inputZoneDbs_:
    ProcessZoneDbf(dbInfo, zoneDb)

for stateDb in args.inputStateDbs_:
    ProcessStateDbf(dbInfo, stateDb)

PostProcess(dbInfo)
