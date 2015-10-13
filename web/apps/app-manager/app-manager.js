/*
 * CLIRA application manager
 *
 * Fetch and install CLIRA apps from a variety of sources. Also view, update 
 * or delete installed apps.
 */

$(function($) {
    var META = ".clira";
    function createApp () {
        var appManager = {
            view : null,
            state : {},
            mode : null,
            appList : null,
            metaDefinition : [
                    { 
                        id : "name", 
                        type : "string", 
                        label : "App name", 
                        disabled : true 
                    },
                    { 
                        id : "version", 
                        type : "string", 
                        label : "Version" 
                    },
                    { 
                        id : "author", 
                        type : "string", 
                        label : "Author" 
                    },
                    { 
                        id : "author-email", 
                        type : "string", 
                        label : "Author email" 
                    },
                    {   
                        id : "app-meta-url", 
                        type : "string", 
                        label : "Metafile URL" 
                    },
                    { 
                        id : "description", 
                        type : "string", 
                        label : "App description"
                    },
                    { 
                        id : "readme", 
                        type : "string", 
                        label : "Readme URL" 
                    },
                    { 
                        id : "files", 
                        type : "array", 
                        label : "App file list" 
                    },
                    { 
                        id : "external", 
                        type : "array", 
                        label : "External files"
                    }
            ],
            setError : function (msg) {
                this.state = { error : msg };
                this.view.set('controller.state', this.state);
                console.error(msg);
            },
            setSource : function (src) {
                var data = {};
                data[src] = true;
                this.setView('src', data);
            },
            setSuccess : function (data) {
                this.state = { success : data };
                this.view.set('controller.state', this.state);             
            },
            setLoading : function (msg) {
                this.state = { loading : msg };
                this.view.set('controller.state', this.state);
            },
            resetState : function () {
                this.state = {};
                this.view.set('controller.state', this.state);             
            },
            setView : function (key, value) {
                this.view.set('controller.' + key, value);
            }, 
            setMode : function (val) {
                this.mode = val;
                var mode = {};
                mode[val] = true;
                this.setView('mode', mode);
            },
            getMeta : function (appName) {
                var def = $.Deferred();
                this.doAjax({
                    name : appName,
                    mode : 'getMeta'
                }, function (res) {
                    if (res.success && res.meta) {
                        var meta = res.meta;
                        var list = [];
                        appManager.metaDefinition.forEach(function(v, i) {
                            var data = {};
                            if (v.type == "array" && !(v.id in meta))
                                return;

                            if (v.id in meta) {
                                data.value = meta[v.id];
                                if (res.metaCreate && v.id == "files") {
                                   data.value.push(appName + META 
                                       + " (Will be created)");
                                }
                            }
                            data.name = v.id;
                            data.label = v.label;
                            data[v.type] = true;
                            if ('disabled' in v)
                                data.disabled = v.disabled;
                            list.push(data);
                        });
                        if (res.metaCreate) 
                            appManager.setView('metaCreate', true);
                        else 
                            appManager.setView('metaCreate', false);
                        appManager.setView('appName', appName);
                        appManager.setView('metaList', list);
                    } else if (res.error) {
                        appManager.setError(res.error);
                    }
                    def.resolve();
                });
                return def.promise();
            },
            doAjax : function (data, onSuccessCb, onErrorCb) {
                if (!onErrorCb) {
                    onErrorCb = function (xhr, stat, msg) {
                        error = "AJAX request failed : ";
                        if (stat == "timeout")
                            error += "Request timed out";
                        else 
                            error += stat;
                        appManager.setError(error);
                    } 
                }
                $.ajax({
                    url : '/apps/app-manager/py/app-manager.py',
                    cache : false,
                    data : data,
                    dataType : 'json',
                    success : function (res) {
                        onSuccessCb(res);
                    },
                    error : onErrorCb,
                    timeout : 10000
                });         
            },
            showAppList : function () {
                var that = this;
                var def = $.Deferred();
                this.doAjax({mode : "listApps"}, function (res) {
                    if (res.success) {
                        that.appList = res.appList;
                        that.setView('appList', res.appList);
                        def.resolve();
                    }
                });
                return def.promise();
            },
            installApp : function (formData, callback) {
                this.setLoading("Installing app...");
                formData.mode = 'installApp';
                this.doAjax(formData, callback);
            },
            updateApp : function (appName, callback) {
                data = {
                    name : appName,
                    mode : 'updateApp'
                };
                this.doAjax(data, callback);
            },
            checkInstallLock : function (callback) {
                data = {
                    mode : 'checkInstallLock'
                }; 
                this.doAjax(data, callback);
            },
            checkForUpdates : function () {
                var that = this;
                this.appList.forEach(function (app, i) {
                    if (app.meta["app-meta-url"]) {
                        that.doAjax({
                            mode : "checkAppUpdate",
                            url : app.meta["app-meta-url"],
                            version : app.meta.version
                        }, function (res) {
                            if (res.success) { 
                                var html = "<button id='update-" + app.name + 
                                    "' class='btn btn-primary update-btn " + 
                                        "btn-sm'>" + 
                                    "Update</button>" + "<label>New version " + 
                                    res.updateInfo['new-version'] + 
                                        " available.";
                                
                                 var uptoDateHtml = "<span class='glyphicon " + 
                                    "glyphicon-ok-circle'></span>" + 
                                        "<b>Up-to-date</b>";
                                if (res.updateInfo['new-version']) {
                                    var newVer = res.updateInfo['new-version'];
                                    that.view.$(".app-status#" + app.name)
                                        .removeClass('app-loading')
                                        .html(html);

                                    that.view.$("#update-" + app.name)
                                        .click(function (e) {

                                        e.preventDefault();
                                        $(this).html("Updating...")
                                            .prop('disabled', true);
                                        $("<div class='app-loading'></div>")
                                            .insertAfter($(this));
                                        
                                        that.updateApp(app.name, function (res){
                                            if (res.success) {
                                                Ember.set(app.meta, 'version',
                                                    newVer);
                                                $.clira.reloadApp(app.name)
                                                    .done(function () {
                                                    that.view.$(".app-status#" 
                                                        + app.name)
                                                        .html(uptoDateHtml)
                                                        .addClass('alert ' + 
                                                            'alert-success');
                                                });
                                            } else if (res.error) {
                                                console.err(res.error);
                                            }
                                        });
                                    });
                                } else {
                                    that.view.$(".app-status#" + app.name)
                                        .removeClass('app-loading')
                                        .html(uptoDateHtml)
                                        .addClass('alert alert-success');
                                }
                            } else if (res.error) {
                                var errorHtml = "<div class='alert " + 
                                    "alert-warning'>" + 
                                    "<span class='glyphicon " + 
                                    "glyphicon-exclamation-sign'></span>" 
                                    + res.error + "</div>";
                                that.view.$(".app-status#" + app.name)
                                        .removeClass('app-loading')
                                        .html(errorHtml);
                            }
                        });
                    }
                });                
            },
            getFormData : function (ctx) {
                var data = {};
                ctx.$("input:text").each(function(i) {
                    if ($(this).prop('disabled') || $(this).val() == "") return;
                    data[$(this).attr('name')] = $(this).val();
                });
                return data;
            }
        }
        return appManager;
    }
    function createViews(app) {
        var views = {
            selectAppSrc : Ember.View.extend({
                render : function (buffer) {
                    var options = 
                        "<option value='localDisk'>Local Disk</option>" + 
                        "<option value='github'>Github</option>" +
                        "<option value='webServer'>Web Server</option>";
                    buffer.push(options);         
                },
                didInsertElement : function () {     
                    var ps = this.get('parentView').$("#selectAppSrc");
                    ps.selectric({
                        onChange : function () {
                            app.setSource(ps.val());
                            app.resetState();
                        }
                    }); 
                }
            }),
            formInput : Ember.View.extend({
                didInsertElement : function () {
                    var that = this;
                    var content = null;
                    this.$("#btn-browse").click(function (e) {
                        e.preventDefault();
                        app.resetState();
                        that.$("#file-input")[0].value = null;
                        that.$("#metaPath").val("");
                        that.$("#file-input").focus().trigger('click');
                    });
                    this.$("#file-input").change(function (e) {
                        if (!this.files.length)return;
                        var path = $(this).val().split(/[/\\]/);
                        that.$("#metaPath").val(path[path.length - 1]);
                        var file = this.files[0];
                        var reader = new FileReader();
                        reader.onload = function (e) {
                            content = reader.result.split(",")[1];
                        }
                        reader.onerror = function (e) {
                            app.setError("Failed to read " + path + " : " +
                                reader.error);
                        }
                        reader.readAsDataURL(file);
                    });
                    this.$("#installApp").click(function (e) {
                        e.preventDefault();
                        if (app.state.success) {
                            app.resetState();
                            that.$("input:text").val("");
                            that.$("input:text:first").focus();
                            $(this).html("Install");
                            return;
                        }
                        var data = app.getFormData(that);
                        data.src = that.get('parentView')
                            .$('#selectAppSrc').val();
                        var metaPath = that.$("#metaPath").val();
                        if (data.src == "localDisk") {
                            if (metaPath == "") {
                                content = false;
                            }
                            var fileInput = that.$("#file-input");
                            if (fileInput && fileInput[0].value) {
                                var path = fileInput.val().split(/[/\\]/);
                                if (content 
                                    && (metaPath == path[path.length - 1])) {
                                    data.b64file = content;
                                }
                            }
                        }
                        var cb = getInstallCb(app, data);
                        app.installApp(data, cb);
                    });
                    this.$("#metaPath").autocomplete({
                        source : "/apps/app-manager/py/list-dirs.py",
                        minLength : 2,
                        select : function (event, ui) {
                            app.resetState();
                        }
                    });
                }                    
            }),
            refreshApps : Ember.View.extend({
                didInsertElement : function () {
                    p = this.get('parentView').$("#appManagerTabs");
                    pHeight = parseInt(p.height());
                    pPos = p.offset();
                    pTop = parseInt(pPos.top);
                    pLeft = parseInt(pPos.left);
                    this.$("#refreshAppList").offset({
                        top : (pTop + pHeight - 15), 
                        left : pLeft
                    });

                    this.$("#refreshAppList").click(function (e){
                        e.preventDefault();
                        app.setView('appList', false);
                        app.showAppList().done(function () {
                            app.checkForUpdates(); 
                        });
                    }); 
                }
            }),
            showApps : Ember.View.extend({
                didInsertElement : function () {
                    this.$("#appManagerTabs").tabs();
                }
            }),
            editMeta : Ember.View.extend({
                didInsertElement : function () {
                    var that = this;
                    var findInMeta = function (list, field) {
                        var obj;
                        list.some(function (v, i) {
                            if (v.name == field) {
                                obj = v;
                                return true;
                            }
                        });
                        return obj;
                    };
                    this.$("#resetMeta").click(function(e) {
                        e.preventDefault();
                        app.resetState();
                        var appName = app.view.get('controller.appName');
                        var controller = app.view.get('controller');
                        Ember.set(controller, 'metaList', false);
                        app.getMeta(appName);
                    });
                    this.$("#refreshFileList").click(function (e) {
                        e.preventDefault();
                        var appName = app.view.get('controller.appName');
                        var metaList = app.view.get('controller.metaList')
                        var data = { 
                            name : appName, 
                            mode : "fetchFileList" 
                        };
                        var files; 
                        app.doAjax(data, function (res) {
                            if (res.success && res.fileList) {
                                var files = findInMeta(metaList, 'files');
                                Ember.set(files, 'value', res.fileList);
                            }
                        });
                    });
                    this.$("#saveMeta").click(function (e) {
                        e.preventDefault();
                        app.resetState();
                        $(this).prop('disabled', true);
                        var btn = this;
                        var appName = app.view.get('controller.appName');
                        data = app.getFormData(that);
                        var metaList = app.view.get('controller.metaList');
                        var files = findInMeta(metaList, 'files');
                        data.files = files.value;
                        data.name = appName;
                        var form = { 
                            meta : JSON.stringify(data), 
                            mode : "saveMeta" 
                        };
                        app.doAjax(form, function (res) {
                            if (res.success) {
                                var controller = app.view.get('controller');
                                Ember.set(controller, 'metaList', false);
                                app.getMeta(appName).done(function () {
                                        app.setSuccess({
                                        msg : "Successfully updated metafile!"
                                    }); 
                                });
                            } else if (res.metaError) {
                                var metaList = app.view.get(
                                    'controller.metaList');
                                var field = findInMeta(metaList, 
                                    res.metaError.name);
                                Ember.set(field, 'error', res.metaError.error);
                            } 
                            $(btn).prop('disabled', false);
                        });
                    });
                }
            })
        }
        return views;
    }
    function appManagerInit (view) {
        var app = createApp();
        app.view = view;
        app.setView('views', createViews(app));
        return app;
    }
    function getInstallCb (ctx, data) {
        var cb = function (res) {
            if (res.error) {
                ctx.setError(res.error);
            } else if (res.wait) {
                ctx.setLoading("Waiting for existing app " + 
                        "installation to finish...");
                var id = setInterval(function () {
                    ctx.checkInstallLock(function (res) {
                        if (res.success) {
                            clearInterval(id);
                            ctx.installApp(data, cb);
                        }
                    });
                }, 2000);
            } else if (res.success) {
                $.clira.reloadApp(res.appName)
                        .done(function () {
                    ctx.setSuccess({
                        name : res.appName,
                        result : 'installed'
                    });
                    that.$("#installApp").html("Install " + 
                        "another app");
                    ctx.showAppList().done(function() {
                        ctx.checkForUpdates();
                    });
                });
            }
        }
        return cb;
    }
    $.clira.commandFile({
        name : "app-manager",
        templatesFile : "/apps/app-manager/app-manager.hbs",
        prereqs : [
            "/external/selectric/selectric.js"
        ],
        commands : [
            {
                command : "show apps",
                help : "List all apps, update or install new apps",
                templateName : "app-manager",
                execute: function (view, cmd, parse, poss) {
                    var app = appManagerInit(view);
                    app.setMode('showApps');
                    app.setSource("localDisk");
                    app.showAppList().done(function () {
                        app.checkForUpdates();
                    });
                }
            },
            {
                command : "install app",
                help : "Install an app via commandline",
                templateName : "app-manager",
                arguments : [
                    {
                        name : "url-or-path",
                        help : "App url or local path to app location",
                        type : "string",
                        nokeyword : true,
                    }
                ],
                execute: function (view, cmd, parse, poss) {
                    var app = appManagerInit(view);
                    var arg = poss.data["url-or-path"];
                    if (arg) {
                        app.setMode("appUpdateCmdl");
                        var opts = {};
                        var a = document.createElement('a');
                        a.href = arg;
                        if (a.hostname == "github.com") {
                            opts.src = "github";
                            opts.url = arg;
                        } else if (a.hostname == window.location.hostname) {
                            opts.src = "localDisk";
                            opts.path = arg;
                        } else {
                            opts.src = "webServer";
                            opts.url = arg;
                        }
                        var cb = getInstallCb(app, opts);
                        app.installApp(opts, cb);
                    } else {
                        /*
                         * With no arg, we default to showing the user the
                         * main app install panel. 
                         */
                        app.setMode('installApps');
                        app.setSource('localDisk');
                    }
                }
            },
            {
                command : "update app",
                help : "Update an app via command line",
                templateName : "app-manager",
                arguments : [
                    {
                        name : "app-name",
                        help : "Name of application to be updated",
                        type : "string",
                        nokeyword : true,
                        mandatory : true
                    }
                ],
                execute: function (view, cmd, parse, poss) {
                    var app = appManagerInit(view);
                    app.setMode("appUpdateCmdl");
                    var appName = poss.data["app-name"];
                    app.setLoading("Updating '" + appName + "' app...");
                    app.updateApp(appName, function (res) {
                        if (res.success) {
                            if (res['up-to-date']) {
                                app.setSuccess({
                                    name : appName,
                                    uptodate : true,
                                }); 
                            } else {
                                $.clira.reloadApp(appName).done(function(){
                                    app.setSuccess({
                                        name : appName,
                                        result : 'updated',
                                        version : res['new-version']
                                    });
                                });
                            }
                        } else if (res.error) {
                            app.setError(res.error);
                        }
                    });
                }
            },
            {
                command : "edit metafile",
                help : "Create or edit an application metafile",
                templateName : "app-manager",
                arguments : [
                    {
                        name : "app-name",
                        help : "Update metafile belonging to app-name",
                        type : "string",
                        nokeyword : true,
                        mandatory : true
                    }
                ],
                execute: function (view, cmd, parse, poss) {
                    var app = appManagerInit(view);
                    app.setMode("editMeta");
                    var appName = poss.data["app-name"];
                    app.getMeta(appName);
                }
            }
        ] 
    });
});
