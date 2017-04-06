#CLIRA app installation core routines.

import urllib2, cgi, os, json, shutil, urlparse, base64, cgitb, sys, traceback
import time, zipfile, tempfile, tarfile
from itertools import izip_longest

#Meta file extension
META = ".clira"

"""
Main wrapper to call various app management  routines depending on params 
passed
"""
def manageApps () :

    #extract the form data
    form = cgi.FieldStorage()
    mode = form.getvalue('mode')

    if mode == "listApps" :
        res = getAppList()
        if res :
            updateResponse('appList', res)
            send("success", True)
    elif mode == "checkAppUpdate" :
        res = getUpdateInfo(form.getvalue('url'), form.getvalue('version'))
        if res : 
            updateResponse('updateInfo', res)
            send("success", True)
    elif mode == "fetchFileList" :
        filelist = getAppFileList(form.getvalue('name'))
        if filelist :
            updateResponse('fileList', filelist)
            send("success", True)
    elif mode == "getMeta" and getMeta(form.getvalue('name')) :
        send("success", True)
    elif mode == "saveMeta" and saveMeta(form.getvalue('meta')) :
        send("success", True)
    elif mode == "updateApp" and updateApp(form.getvalue('name')):
        send("success", True)
    elif mode == "installApp" and installApp(form) :
        send("success", True)
    elif mode == "checkInstallLock" and checkInstallLock() :
        send("success", True)


"""
List installed apps and check for update
"""
def getAppList () :
    appDir = appDirPath()
    appList = []
    dirs = os.listdir(appDir)
    for d in dirs :
        app = {}
        if not d.startswith(('.', '..')) and os.path.isdir(appDir + '/' + d):
            app['name'] = d
            metapath = "%s/%s/%s%s" % (appDir, d, d, META)
            if not os.path.isfile(metapath) :
                app['meta'] = False
                appList.append(app)
                continue
            try : 
                with open(metapath) as fh :
                    meta = json.load(fh)
                    ret = validateMetaData(meta)
                    if 'error' in ret :
                        app['meta-error'] = ret['error']
                        app['meta'] = meta
                        appList.append(app)
                        continue
                    if 'app-meta-url' not in meta :
                        app['meta-error'] = "No app Update URL"
                    fh.close()
            except (ValueError, IOError) as e :
                app['meta'] = False
                app['meta-error'] = "Failed to load meta file for '%s' : %s" \
                        % (d, str(e))
                appList.append(app)
                continue
            app['meta'] = meta
            appList.append(app)
    return appList

def updateApp (appName) :
    path = "%s/%s%s" % (appDirPath(appName), appName, META)
    if not os.path.exists(path) :
        error("Cannot update '%s'. App not installed or meta file " \
                "is missing." % appName)
        return False
    meta = getAppMetaData('fileDisk', path)
    if not meta :
        return False
    if 'app-meta-url' not in meta :
        error("Cannot update '%s', missing 'app-meta-url' from meta file" \
                % appName)
        return False
    url = meta['app-meta-url']
    info = getUpdateInfo(url, meta['version'])
    if not info:
        return False
    if not info['new-version'] :
        updateResponse('up-to-date', True)
        return True
    updateResponse('new-version', info['new-version'])
    if not githubInstallCommon(info['meta'], 'update') :
        shutil.rmtree(appDirPath("_" + appName), ignore_errors=True)
        return False
    
    return True

"""
Check if an app has a newer version available using the URL given by the client
"""
def getUpdateInfo (url, version) :
    updateInfo = {}
    if not url :
        updateInfo['error'] = "Missing url field"
        return updateInfo
    if urlparse.urlparse(url).netloc == "github.com" :
        src = "github"
    else : 
        src = None
    meta = getAppMetaData(src, url)
    if meta :
        if 'not-modified' in meta :
            updateInfo['new-version'] = False
        else :
            if compareVersions(version, meta['version']) == -1 :
                updateInfo['new-version'] = meta['version']
            else :
                updateInfo['new-version'] = False
            updateInfo['meta'] = meta
    else :
        return False
    return updateInfo

"""
Compare versions v1, v2 and return :
    -1 if v1 < v2
     1 if v1 > v2
     0 if v1 == v2
"""
def compareVersions (v1, v2) :
    ver1 = (int(n) for n in v1.split('.'))
    ver2 = (int(n) for n in v2.split('.'))
    for i, j in izip_longest(ver1, ver2, fillvalue=0) :
        if i > j :
            return 1
        elif i < j :
            return -1
    return 0

def installApp (form) :
    if not getInstallLock() :
        return False
    src = form.getvalue('src')
    if (src == "localDisk") :
        return installFromDisk(form)
    elif (src == "github"):
        return installFromGithub(form)
    elif (src == "webServer") :
        return installFromWebServer(form)

"""
Install an app from disk using the params
"""
def installFromDisk (form) :
    try : 
        apath = path = tmpDir = appName = tmpArchive = dontClearLock = None
        if 'b64file' in form:
            b64file = form.getvalue('b64file')
            archive = base64.b64decode(b64file);
            tmpArchive = tempfile.mkstemp(dir=appDirPath(), text=False)
            apath = tmpArchive[1] #absolute path to the temp archive
            with open(apath, 'w') as fh :
                fh.write(archive)
                fh.close()
        else :
            path = form.getvalue('path')
            if (not path 
                    or not os.path.isfile(path) 
                    or not os.path.basename(path).endswith((META, 'tgz', 'zip'))):
                error("Invalid app install file or archive")
                return False
            if (os.path.basename(path).endswith(META)) :
                appName = os.path.basename(path).split('.')[0]
                if appName == "" :
                    error("Invalid app install file name")
                    return False
                if appInstalled(appName) :
                    error("'%s' app is already installed." % appName)
                    return False
                
            if tarfile.is_tarfile(path) or zipfile.is_zipfile(path) :
                apath = path
            else : 
                metafile = path

        if apath :
            tmpDir = extractAppArchive(apath)
            if not tmpDir :
                return False
            if tmpDir[-1] == '/':
                tmpDir = tmpDir[:-1]
            appName = os.path.basename(tmpDir).replace("_", "")
            metafile = "%s/%s/%s%s" % (tmpDir, appName, appName, META)
            if not os.path.isfile(metafile) :
                raiseEx("Archive does not contain app meta file")
            if (appInstalled(appName)):
                raiseEx("'%s' app is already installed" % appName)
         
        updateResponse('appName', appName)
        meta = getAppMetaData('fileDisk', metafile)
        if not meta:
            raiseEx()
        srcDir = os.path.dirname(metafile)
        destDir = appDirPath(appName)
        files = meta["files"]

        # Create the necessary app directories and
        # Copy all the app files to the dest
        if (not createAppDirs(destDir, files) 
                or not copyAppFiles(srcDir, destDir, files)):
            raiseEx()
        return True
    except (IOError, OSError, ValueError, AppManagerError) as e : 
        if e :
            error(str(e))
        return False
    finally :
        if tmpDir and os.path.exists(tmpDir) :
            shutil.rmtree(tmpDir, ignore_errors=True)
        if tmpArchive and os.path.exists(tmpArchive[1]) :
            os.remove(tmpArchive[1])
        if not dontClearLock :
            clearInstallLock(appName)

"""
Use the app meta data and fetch application files using the Github API. 
Download files directly into the destination directory. For each 'file' in the
app meta 'files' field, we create the corresponding Github API URL and fetch
the data. The file content is contained base64 encoded in the 'content' field.
Take that and write to dest location.
"""
def installFromGithub (form) :
    appName = None
    try :
        url = form.getvalue('url')
        if not url :
            url = form.getvalue('app-meta-url')
        if not url :
            error("No app src URL defined.")
            return False
        appName = getAppNameFromURL(url)
        if not appName :
            return False
        updateResponse('appName', appName)
        if appInstalled(appName) :
            error("'%s' is already installed." % appName)
            return False
        meta = getAppMetaData("github", url)
        if not meta :
            return False
        if not githubInstallCommon(meta, 'install') :
            return False
        return True
    except Exception as e :
        error("Failed to install app from Github : %s" % str(e))
        return False
    finally :
        if appName and os.path.exists(appDirPath("_" + appName)) :
            shutil.rmtree(appDirPath("_" + appName), ignore_errors=True)
        clearInstallLock(appName)

"""
Common set of tasks while installing or updating an app from Github
"""
def githubInstallCommon (meta, mode) :
    files = meta["files"]
    appName = meta['name']
    destDir = appDirPath("_" + appName)

    if not createAppDirs(destDir, files):
        error("Failed to create app directories")
        return False

    #First write the meta file, this prevents an unnecessary api call later
    if not updateAppMetaFile(meta, True):
        return False

    dirUrl = getDirUrl(meta['app-meta-url'])
    if dirUrl[-1] != '/' :
        dirUrl += '/'

    for f in files :
        # Skip writing the meta file, its handled already
        if f.lower().endswith(META):
            continue
        fUrl = getGithubApiUrl(dirUrl + f)
        if not fUrl :
            return False
        fdata = githubApiGetBlob(fUrl)
        if not fdata :
            return False
        if not writeFile(destDir + '/' + f, fdata) :
            return False
    if mode == "update" :
        rmAppDir(appName)
    os.rename(destDir, appDirPath(appName))
    return True 

"""
Install an app from a webserver URL
"""
def installFromWebServer (form) :
    appName = None
    try :
        url = form.getvalue('url')
        if not url :
            error("Invalid URL")
            return False
        appName = getAppNameFromURL(url)
        if not appName :
            return False
        updateResponse('appName', appName)
        meta = getAppMetaData("webServer", url)
        if not meta :
            return False
        destDir = appDirPath(meta['name'])
        files = meta['files']
        rmAppDir(appName)
        if not createAppDirs(destDir, files) :
            return False

        #First write the meta file, this prevents an unnecessary api call later
        if not updateAppMetaFile(meta, True):
            return False

        dirUrl = getDirUrl(url)
        if dirUrl[-1] != '/' :
            dirUrl += '/'
        for f in files :
            # Skip writing the meta file, its handled already
            if f.lower().endswith(META):
                continue
            res = getURL(dirUrl + f)
            if ('code' or 'reason') in res :
                printHttpError(res, json_content=False)
                return False
            fdata = res['content']
            if not fdata or not writeFile(destDir + '/' + f, fdata) :
                return False
        return True
    except Exception as e :
        error("Failed to install app from web server : %s" % str(e))
        return False
    finally : 
        clearInstallLock(appName)
    
"""
Write 'data' to 'dest' file path 
"""
def writeFile (dest, data) :
    try :
        with open(dest, "w") as fh :
            fh.write(data)
            fh.close()
            return True
    except (IOError, OSError) as e : 
        error("Failed to write file to %s : %s" % (dest, e(str)))
    return False

"""
Return the new app directory relative to where we are. This should be changed
to using a CLIRA env variable that stores the clira apps directory rather 
than using this ugly method.
"""
def appDirPath (dirname=None) : 
    if not dirname :
        return os.path.realpath("../../")
    return "%s/%s" % (os.path.realpath("../../"), dirname)

def appMetaFile(appName) :
    return "%s/%s/%s%s" % (appDirPath(), appName, appName, META)

def getDirUrl (url) :
    o = urlparse.urlparse(url)
    return urlparse.urlunparse((o.scheme, o.netloc, os.path.dirname(o.path), 
        None, None, None))

def appInstalled (appName) :
    return os.path.exists(appDirPath(appName))

def checkInstallLock () :
    if os.path.exists("%s/install.lock" % appDirPath()) :
        send('wait', True)
        return False
    return True

def getInstallLock () :
    if os.path.exists("%s/install.lock" % appDirPath()) :
        send('wait', True)
        return False
    try :
        open("%s/install.lock" % appDirPath(), 'a').close()
        return True
    except (IOError) as e :
        error("Failed to acquire install lock : %s" % str(e))
        return False
"""
Maybe have parallel app installs in the future
def updateInstallLock (appName) :
    try :
        if (os.path.exists("%s/%s.install.lock" % (appDirPath(), appName))):
            error("'%s' app is already being installed." % appName)
            return False
        elif (os.path.exists("%s/install.lock" % appDirPath())) :
            os.rename('%s/install.lock' % appDirPath(), 
                    "%s/%s.install.lock" % (appDirPath(), appName))
            return True
    except Exception as e :
        error("Failed to update install lock for '%s' app : %s" 
                % (appName, str(e)))
        return False
"""
def clearInstallLock (appName=None) :
    path = appDirPath()
    try :
        if appName and os.path.exists("%s/%s.install.lock" % (path, appName)) :
            os.remove("%s/%s.install.lock" % (path, appName))
        elif os.path.exists("%s/install.lock" % path) :
            os.remove("%s/install.lock" % path)
        return True
    except Exception as e :
        error("Failed to clear install lock : %s" % e.strerror)
        return False

def getAppNameFromURL (url) :
    o = urlparse.urlparse(url)
    fname = os.path.basename(o.path)
    if fname.lower().endswith(META):
        (appName, ext) = fname.split('.')
        if appName :
            return appName
    error("Cannot extract app name from URL. Make sure the URL " \
            "points to an app%s file" % META)
    return False

def getAppVersion (appName) :
    appMeta = "%s/%s/%s%s" % (appDirPath(), appName, appName, META)
    try :
        with open(appMeta) as fh :
            meta = json.load(fh)
            if 'version' in meta :
                return meta['version']
            fh.close()
    except (ValueError, IOError) as e :
        error(str(e))
    return False

def encodeUserData(user, password):
    return "Basic " + (user + ":" + password).encode("base64").rstrip()

"""
Convert a github.com URL to a api.github.com URL. Its easier to copy paste a
github.com URL from the browser. We simply parse its components and create an
equivalent api.github.com URL which allows us to fetch more info about the 
resource.
"""
def getGithubApiUrl (url) :
    try :
        o = urlparse.urlparse(url)
        if not o.netloc == "github.com":
            error("Enter a valid Github URL for the file")
            return False
        (owner, repo, ctype, branch, path) = o.path[1::].split('/', 4)
        if branch == 'master' :
            return "https://api.github.com/repos/%s/%s/contents/%s" \
                    % (owner, repo, path)
        else :
            return "https://api.github.com/repos/%s/%s/contents/%s?ref=%s" \
                    % (owner, repo, path, branch)
    except ValueError as e :
        error("Failed to get Github API URL : %s" % str(e))
    return False

"""
Fetch data using a URL. dataType is json if json decoded object is required as 
result. username and password is optional and is used for HTTP auth.
"""
def getURL (url, dataType = None, username = None, password=None, header=None) :
    try :
        req = urllib2.Request(url)
        if dataType == "json" :
            req.add_header('Accept', 'application/json')
        else :
            req.add_header('Accept', 'text/plain')
        req.add_header("Content-type", "application/x-www-form-urlencoded")
        if username and password :
            req.add_header('Authorization', encodeUserData(username, password))
        if header :
            for k in header.keys():
                req.add_header(k, header[k])
        res = urllib2.urlopen(req)
        if dataType == "json" :
            return { 'content' : json.loads(res.read()),
                     'info' : dict(res.info())
                   }
        else :
            return { 'content' : res.read(),
                     'info' : dict(res.info())
                   }
    except urllib2.HTTPError as e :
        data = {}
        data['code'] = e.code
        data['reason'] = e.reason
        content = e.read()
        if content :
            data['content'] = content
        return data
    except urllib2.URLError as e :
        data = {}
        data['reason'] = str(e.reason)
        return data
    except Exception as e :
        error("Failed to fetch URL : %s" % str(e))

"""
Query the Github API using the url and return the file/blob content. Throw an
error if we don't see what we want.
"""
def githubApiGetBlob (url, user=None, password=None) :
    res = getURL(url, "json")
    if not res :
        return False
    if ('code' or 'reason') in res :
        printHttpError(res)
        return False
    res = res['content']
    if 'type' not in res or res['type'] != "file":
        error("URL does not point to a file : type => %s" % res["type"])
        return False
    if "content" not in res :
        error("Missing file content in response")
        return False
    try : 
        return base64.b64decode(res["content"])
    except ValueError as e:
        error("Failed to parse file content : %s" % str(e))
    return False


"""
Check if a resource has been modified using GITHUB's conditional access API.
If the resource has changed, return the resource else return False.
"""
def githubGetIfModified (url, lastModified) :
    header = {}
    if lastModified :
        header['If-Modified-Since'] = lastModified
    res = getURL(url, 'json', None, None, header)
    if ('code' or 'reason') in res :
        if res['code'] == 304 :
            return { 'not-modified' : True }
        else :
            printHttpError(res)
            return False
    return res

"""
Validate mandatory meta data fields
"""
def validateMetaData (meta) :
    fields = ('name', 'version', 'files')
    for f in fields :
        if f not in meta :
            return { 
                    "error" : "meta file missing mandatory field '%s'" % f,
                    "name" : f
                   }
    return {}

def updateAppMetaFile (data, temp=False):
    if temp :
        path = "%s/_%s/%s%s" % (appDirPath(), data['name'], data['name'], META)
    else :
        path = appMetaFile(data['name'])
    try :
        with open(path, 'w') as fh :
            json.dump(data, fh, indent=4)
            fh.close()
            return True
    except Exception as e :
        error("Failed to update app meta file : %s" % str(e))
    return False

def getLastModified (path) :
    try :
        if os.path.isfile(path) :
            mtime = time.gmtime(os.path.getmtime(path))
            return time.strftime("%a, %d %b %Y %H:%M:%S GMT", mtime)
    except Exception as e :
       error(str(e))
    return None


def getAppMetaData (src, url) :
    try :
        if src == "github" :
            url = getGithubApiUrl(url)
            if not url :
                return False
            appName = getAppNameFromURL(url)
            if not appName:
                return False
            path = appMetaFile(appName)
            lastModified = getLastModified(path)
            res = githubGetIfModified(url, lastModified)
            if not res :
                return False
            if 'not-modified' in res :
                return { 'not-modified' : True }
            info = res['info']
            res = res['content']
            if 'content' in res : 
                meta = json.loads(base64.b64decode(res["content"]))
            else :
                error("Failed to find meta file 'content' in response")
                return False

        elif src == "fileDisk" :
            if not os.path.isfile(url):
                error("Invalid file path : %s" % url)
                return False
            with open(url, 'r') as fh :
                meta = json.load(fh)
                fh.close()
        else :
            res = getURL(url, 'json')
            if ('reason' or 'code') in res :
                printHttpError(res, json_content=False)
                return False
            meta = res['content']

        if meta :
            ret = validateMetaData(meta)
            if 'error' in ret:
                error(ret['error'])
                return False
            else :
                return meta
    except ValueError as e : 
        error("Failed to parse meta file : %s" % str(e))
    except IOError as e :
        error("Failed to read meta file : %s" % str(e))
    return False 

def createAppDirs (destDir, files) :
    if not mkdirs(destDir):
        return False
    seen = {}
    for f in files :
        path = os.path.dirname(f)
        if not path or path in seen : continue
        seen[path] = True
        if not mkdirs("%s/%s" % (destDir, path)) :
            return False
    return True

def extractAppArchive (path) :
    try :
        isTar = isZip = None
        if tarfile.is_tarfile(path) :
            isTar = True
        elif zipfile.is_zipfile(path) :
            isZip = True
        else :
            error("Unsupported archive type, only tar and zip allowed")
            return False
        if isTar :
            tfo = tarfile.open(path)
            alist = tfo.getnames()
        else :
            zfo = zipfile.ZipFile(path, 'r')
            alist = zfo.namelist()
        if len(alist) <= 1 :
            error("Archive does not contain any app files")
            return False
        #Prevent writing to root directory via bogus filenames
        for f in alist :
            toks = f.split('/')
            for i, tok in enumerate(toks) :
                if ((not tok and i == 0) or tok.startswith(('..', '.'))):
                    error("Archive contains invalid filename : %s" % f)
                    return False
        if alist[0][-1] == '/':
            appName = alist[0][:-1]
        else :
            appName = alist[0]
        tempDir = appDirPath("_" + appName)
        if not mkdirs(tempDir):
            return False
        if isTar :
            tfo.extractall(tempDir)
        else :
            zfo.extractall(tempDir)
        return tempDir

    except Exception as e :
        error(traceback.format_exc())
        if tempDir and os.path.exists(tempDir) :
            shutil.rmtree(tempDir, ignore_errors=True)
        return False


def rmAppDir(appName) :
    if appInstalled(appName) :
        shutil.rmtree(appDirPath(appName), ignore_errors=True)

def copyAppFiles (srcDir, destDir, files):
    try :
        for f in files :
            shutil.copy("%s/%s" % (srcDir, f), "%s/%s" % (destDir, f))
    except (OSError, IOError) as e : 
        error("Error copying file %s : %s" % (f, str(e)))
        return False
    return True

def printHttpError (data, json_content=True) :
    try :
        msg = None
        if 'reason' in data and not 'code' in data :
            error("HTTP error : %s" % data['reason'])
            return False
        if 'code' in data :
            if 'content' not in data :
                return False
            if json_content :
                content = json.loads(data['content'])
            else :
                content = data['content']
            if 'message' in content :
                msg = content['message']
            if 'documentation_url' in content :
                msg = "%s More info : %s" % (msg, content['documentation_url'])
            if msg :
                error("Github API error: %s" % msg)
            else :
                error("HTTP error : %s %s" 
                        % (str(data['code']), data['reason']))
    except ValueError as e :
        error(str(e))

def mkdirs (path) : 
    try :
        if not os.path.exists(path) :
            os.makedirs(path)
    except Exception as e: 
        error(str(e))
        return False
    return True

def getAppFileList (appName) :
    fileList = []
    try : 
        appDir = appDirPath(appName)
        for root, dirs, files in os.walk(appDir):
            for f in files:
                path = os.path.join(root, f)
                path = path.replace(appDir + '/', "")
                fileList.append(path) 
        return fileList
    except Exception as e:
        error(str(e))
    return False


def getMeta(appName) :
    if not appInstalled(appName) :
        error("No such app installed")
        return False
    if not os.path.isfile(appMetaFile(appName)) :
        meta = {}
        meta['name'] = appName
        meta['files'] = getAppFileList(appName)
        updateResponse('metaCreate', True)
        updateResponse('meta', meta)
        return True
    try :
        with open(appMetaFile(appName)) as fh :
            meta = json.load(fh)
            res = validateMetaData(meta)
            if 'error' in res :
                error(res['error'])
                return False
            fh.close()
            updateResponse('meta', meta)
            return True
    
    except Exception as e :
        error(str(e))
    return False

def saveMeta (metaStr) : 
    try : 
        meta = json.loads(metaStr)
        res = validateMetaData(meta)
        if 'error' in res :
            send('metaError', res)
            return False
        appMeta = meta['name'] + META
        for i, f in enumerate(meta['files']) :
            if f.find("(Will be created)") > -1 :
                meta['files'][i] = appMeta
                break
        if not updateAppMetaFile(meta) :
            return False
        return True
    except ValueError as e :
        error(e(str))
    return False

"""
Update the global response data
"""
def updateResponse (key, value) :
    global response
    response[key] = value

def printResponse () :
    global response
    print json.dumps(response)
    sys.stdout.flush()

def error (msg) :
    updateResponse('error', msg)
    printResponse()

def send (key, value) :
    updateResponse(key, value)
    printResponse()

def raiseEx (msg=None) :
    raise AppManagerError(msg)
    

"""
Simple Generic exception class to handle custom exceptions
"""
class AppManagerError (Exception) :
    pass

#Main : Let's do this!

cgitb.enable()
print "Content-Type: application/json\n\n"

#Our global response data which is sent to the client
response = {}

try :
    manageApps()   
except : 
    #For unknown erros we simply send over the traceback to the client
    error(traceback.format_exc())
