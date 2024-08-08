import argparse
import datetime
import git
import json
import os
import pathlib
import sys

class Keys:
    BuildNumber   = "build_number"
    CommitString  = "commit_string"
    CopyrightYear = "copyright_year"
    ResourceDir   = "resource_dir"
    VersionCommas = "version_commas"
    VersionString = "version_string"

class VersionInfo:
    def __init__(self):
        self.buildNumber_   = None
        self.commitString_  = None
        self.copyrightYear_ = None
        self.resourceDir_   = None
        self.versionCommas_ = None
        self.versionString_ = None

    def __eq__(self, other):
        if isinstance(other, VersionInfo):
            return self.buildNumber_ == other.buildNumber_ and \
                self.commitString_ == other.commitString_ and \
                self.copyrightYear_ == other.copyrightYear_ and \
                self.resourceDir_ == other.resourceDir_ and \
                self.versionString_ == other.versionString_

    def Calculate(self):
        self.versionCommas_ = self.versionString_.replace('.', ',')

    def Value(self, key):
        match key:
            case Keys.BuildNumber:
                return self.buildNumber_
            case Keys.CommitString:
                return self.commitString_
            case Keys.CopyrightYear:
                return self.copyrightYear_
            case Keys.ResourceDir:
                return self.resourceDir_
            case Keys.VersionCommas:
                return self.versionCommas_
            case Keys.VersionString:
                return self.versionString_
            case _:
                return None

kKeys_ = [Keys.BuildNumber,
          Keys.CommitString,
          Keys.CopyrightYear,
          Keys.ResourceDir,
          Keys.VersionCommas,
          Keys.VersionString]

def ParseArguments():
    parser = argparse.ArgumentParser(description='Generate versions')
    parser.add_argument("-c", "--cache",
                        metavar  = "filename",
                        help     = "cache file",
                        dest     = "cache_",
                        default  = None,
                        type     = pathlib.Path)
    parser.add_argument("-b", "--build-number",
                        metavar  = "value",
                        help     = "build number",
                        dest     = "buildNumber_",
                        default  = 0,
                        type     = str)
    parser.add_argument("-g", "--git-repo",
                        metavar  = "path",
                        help     = "base git repository path",
                        dest     = "gitRepo_",
                        default  = os.getcwd(),
                        type     = pathlib.Path)
    parser.add_argument("-i", "--input_header",
                        metavar  = "filename",
                        help     = "input header template",
                        dest     = "inputHeader_",
                        type     = pathlib.Path,
                        required = True)
    parser.add_argument("-o", "--output_header",
                        metavar  = "filename",
                        help     = "output header",
                        dest     = "outputHeader_",
                        type     = pathlib.Path,
                        required = True)
    parser.add_argument("--input-resource",
                        metavar  = "filename",
                        help     = "input resource template",
                        dest     = "inputResource_",
                        default  = None,
                        type     = pathlib.Path)
    parser.add_argument("-r", "--output-resource",
                        metavar  = "filename",
                        help     = "output resource",
                        dest     = "outputResource_",
                        default  = None,
                        type     = pathlib.Path)
    parser.add_argument("-v", "--version",
                        metavar  = "version",
                        help     = "version string",
                        dest     = "version_",
                        type     = str,
                        required = True)
    return parser.parse_args()

def CollectVersionInfo(args):
    print("Collecting version info")

    versionInfo = VersionInfo()

    repo = git.Repo(args.gitRepo_, search_parent_directories = True)

    commitString = str(repo.head.commit)[:10]

    if not repo.is_dirty(submodules = False):
        copyrightYear = datetime.datetime.fromtimestamp(repo.head.commit.committed_date).year
    else:
        commitString  = commitString + "+dirty"
        copyrightYear = datetime.date.today().year

    resourceDir = str(args.gitRepo_).replace("\\", "\\\\")

    versionInfo.buildNumber_   = args.buildNumber_
    versionInfo.commitString_  = commitString
    versionInfo.copyrightYear_ = copyrightYear
    versionInfo.resourceDir_   = resourceDir
    versionInfo.versionString_ = args.version_

    versionInfo.Calculate()

    print("Build Number: " + str(versionInfo.buildNumber_))
    print("Commit String: " + str(versionInfo.commitString_))
    print("Copyright Year: " + str(versionInfo.copyrightYear_))
    print("Version String: " + str(versionInfo.versionString_))

    return versionInfo

def LoadCache(args):
    print("Loading cache")

    cache = None

    try:
        with open(args.cache_) as f:
            data  = json.load(f)
            cache = VersionInfo()
            if Keys.BuildNumber in data:
                cache.buildNumber_ = data[Keys.BuildNumber]
            if Keys.CommitString in data:
                cache.commitString_ = data[Keys.CommitString]
            if Keys.CopyrightYear in data:
                cache.copyrightYear_ = data[Keys.CopyrightYear]
            if Keys.ResourceDir in data:
                cache.resourceDir_ = data[Keys.ResourceDir]
            if Keys.VersionString in data:
                cache.versionString_ = data[Keys.VersionString]
            cache.Calculate()
    except Exception as ex:
        # Ignore error if cache is not found
        pass

    return cache

def WriteTemplate(versionInfo: VersionInfo, inputFile, outputFile):
    try:
        pathlib.Path(outputFile).parent.mkdir(exist_ok=True, parents=True)
        with open(inputFile) as fi, open(outputFile, 'w') as fo:
            for line in fi:
                for key in kKeys_:
                    line = line.replace("${" + key + "}", str(versionInfo.Value(key)))
                fo.write(line)
    except Exception as ex:
        print("Error writing header: " + repr(ex))
        return False

    return True

def WriteHeader(versionInfo: VersionInfo, args):
    print("Writing header")
    return WriteTemplate(versionInfo, args.inputHeader_, args.outputHeader_)

def WriteResource(versionInfo: VersionInfo, args):
    if args.inputResource_ == None or args.outputResource_ == None:
        return None
    print("Writing resource")
    return WriteTemplate(versionInfo, args.inputResource_, args.outputResource_)

def UpdateCache(versionInfo: VersionInfo, args):
    print("Updating cache")

    data = {}
    data[Keys.BuildNumber]   = versionInfo.buildNumber_
    data[Keys.CommitString]  = versionInfo.commitString_
    data[Keys.CopyrightYear] = versionInfo.copyrightYear_
    data[Keys.ResourceDir]   = versionInfo.resourceDir_
    data[Keys.VersionString] = versionInfo.versionString_

    try:
        pathlib.Path(args.cache_).parent.mkdir(exist_ok=True, parents=True)
        with open(args.cache_, 'w') as f:
            json.dump(data, f, indent=4)
    except Exception as ex:
        print("Error updating cache: " + repr(ex))

def main() -> int:
    status = 0

    args        = ParseArguments()
    versionInfo = CollectVersionInfo(args)
    cache       = LoadCache(args)

    if versionInfo == cache and \
       (args.outputHeader_ is None or os.path.exists(args.outputHeader_)) and \
       (args.outputResource_ is None or os.path.exists(args.outputResource_)):
        print("No version changes detected")
    else:
        writeHeaderStatus   = WriteHeader(versionInfo, args)
        writeResourceStatus = WriteResource(versionInfo, args)

        if writeHeaderStatus or writeResourceStatus:
            UpdateCache(versionInfo, args)
        if writeHeaderStatus == False or writeResourceStatus == False:
            status = -1

    return status

if __name__ == "__main__":
    sys.exit(main())
