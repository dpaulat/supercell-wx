import argparse
import datetime
import git
import json
import os
import pathlib

class Keys:
    CommitString  = "commit_string"
    CopyrightYear = "copyright_year"
    VersionString = "version_string"

class VersionInfo:
    def __init__(self):
        self.commitString_  = None
        self.copyrightYear_ = None
        self.versionString_ = None

    def __eq__(self, other):
        if isinstance(other, VersionInfo):
            return self.commitString_ == other.commitString_ and \
                self.copyrightYear_ == other.copyrightYear_ and \
                self.versionString_ == other.versionString_
    
    def Value(self, key):
        match key:
            case Keys.CommitString:
                return self.commitString_
            case Keys.CopyrightYear:
                return self.copyrightYear_
            case Keys.VersionString:
                return self.versionString_
            case _:
                return None

kKeys_ = [Keys.CommitString, Keys.CopyrightYear, Keys.VersionString]

def ParseArguments():
    parser = argparse.ArgumentParser(description='Generate versions')
    parser.add_argument("-c", "--cache",
                        metavar  = "filename",
                        help     = "cache file",
                        dest     = "cache_",
                        default  = None,
                        type     = pathlib.Path)
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
    
    versionInfo.commitString_  = commitString
    versionInfo.copyrightYear_ = copyrightYear
    versionInfo.versionString_ = args.version_

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
            if Keys.CommitString in data:
                cache.commitString_ = data[Keys.CommitString]
            if Keys.CopyrightYear in data:
                cache.copyrightYear_ = data[Keys.CopyrightYear]
            if Keys.VersionString in data:
                cache.versionString_ = data[Keys.VersionString]
    except Exception as ex:
        # Ignore error if cache is not found
        pass

    return cache

def WriteHeader(versionInfo: VersionInfo, args):
    print("Writing header")
    
    try:
        pathlib.Path(args.outputHeader_).parent.mkdir(exist_ok=True, parents=True)
        with open(args.inputHeader_) as fi, open(args.outputHeader_, 'w') as fo:
            for line in fi:
                for key in kKeys_:
                    line = line.replace("${" + key + "}", str(versionInfo.Value(key)))
                fo.write(line)
    except Exception as ex:
        print("Error writing header: " + repr(ex))
        return False

    return True

def UpdateCache(versionInfo: VersionInfo, args):
    print("Updating cache")

    data = {}
    data[Keys.CommitString]  = versionInfo.commitString_
    data[Keys.CopyrightYear] = versionInfo.copyrightYear_
    data[Keys.VersionString] = versionInfo.versionString_

    try:
        pathlib.Path(args.cache_).parent.mkdir(exist_ok=True, parents=True)
        with open(args.cache_, 'w') as f:
            json.dump(data, f, indent=4)
    except Exception as ex:
        print("Error updating cache: " + repr(ex))

args        = ParseArguments()
versionInfo = CollectVersionInfo(args)
cache       = LoadCache(args)

if versionInfo == cache:
    print("No version changes detected")
else:
    if WriteHeader(versionInfo, args):
        UpdateCache(versionInfo, args)
