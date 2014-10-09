/*
 *  -*-  indent-tabs-mode:nil -*-
 * Copyright 2014, Juniper Network Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

/*
 * Ember views for modifying CLIRA preferences
 */

/*
 * View to display preferences dialog and handle devices, group and general
 * preferences
 */
Clira.PreferencesDialog = Ember.View.extend({
    templateName: "preferences",
    isVisible: false,

    actions: {
        devicesPref: function() {
            var dpv = this.get('parentView').get('parentView')
                          .container.lookup('view:DevicesPref');
            this.get('parentView').get('parentView').pushObject(dpv);
        },
        generalPref: function() {
            var prefs = Clira.Preference.find(),
                fields = [];

            // Sanitize before creating a form
            if (prefs) {
                prefs.forEach(function(item) {
                    pref = item.__data;
                    if (pref.type == "boolean") {
                        pref['boolean'] = true;

                        if (pref.value == "true") {
                            pref['value'] = true;
                        } else {
                            pref['value'] = false;
                        }
                    } else {
                        pref['boolean'] = false;
                    }
                    fields.push(pref);
                });
            }
            var gpv = this.get('parentView').get('parentView')
                          .container.lookup('view:GeneralPref');
            gpv.fields = fields;
            this.get('parentView').get('parentView').pushObject(gpv);
        },
        groupsPref: function() {
            var gpv = this.get('parentView').get('parentView')
                          .container.lookup('view:GroupsPref');
            this.get('parentView').get('parentView').pushObject(gpv);
        }
    },

    /*
     * We use jqGrid to read device and group config from db and display 
     * them in corresponding preferences modal
     */
    didInsertElement: function() {
        var elementId = this.get('parentView').elementId;
        $('#' + this.elementId + ' .prefs-main-form').dialog({
            appendTo: '#' + elementId + ' .output-content',
            dialogClass: 'inline-dialog',
            draggable: false,
            position: { 
                my: 'left', 
                at: 'left', 
                of: '#' + elementId 
            },
            resizable: false,
            width: 220,
        });
    }
});

/*
 * View that allows user to add/edit device groups
 */
Clira.GroupsPrefView = Ember.View.extend({
    templateName: 'groups_pref',
    classNames: ['output-content'],
    
    didInsertElement: function() {
        var view = this;
        /* Set Up Groups */
        $('#prefs-groups-form').dialog({
            dialogClass: 'inline-dialog',
            appendTo: '#' + view.elementId,
            resizable: false,
            draggable: false,
            position: {
                my: 'left',
                at: 'left',
                of: '#' + view.elementId
            },
            buttons: {
                'Close': function() {
                    view.destroy();
                }
            },
            width: 600
        });

 
        $('#' + view.elementId + ' #prefs-groups-form #prefs-groups-grid')
            .jqGrid({
            url: '/clira/system/db.slax?p=group_list',
            editurl: '/clira/system/db.slax?p=group_edit',
            datatype: 'json',
            colNames: ['Name', 'Members', ''],
            colModel: [
                {
                    name: 'name',
                    index: 'name',
                    width: 90,
                    editable: true,
                    qTip: "The name of this group to be identified by in CLIRA.",
                    editrules: {
                        required: true
                    }
                },
                {
                    name: 'members',
                    index: 'devices',
                    editable: true,
                    qTip: "The devices that are a member of this group.",
                    edittype: 'select',
                    editrules: {
                        required: true
                    },
                    editoptions: {
                        multiple: true,
                        dataUrl: '/clira/system/db.slax?p=devices',
                        buildSelect: function (data) {
                            var j = $.parseJSON(data);
                            var s = '<select>';
                            if (j.devices && j.devices.length) {
                                $.each(j.devices, function (i, item) {
                                    s += '<option value="' + item.id + '">' 
                                        + item.name + '</option>';
                                });
                            }
                            return $(s)[0];
                        }
                    }
                },
                {
                    name: 'action',
                    index: 'action',
                    width: 40,
                    formatter: 'actions',
                    formatoptions: {
                        editformbutton: true,
                        editOptions: {
                            closeAfterEdit: true,
                            afterShowForm: function ($form) {
                                var $dialog = $('#editmodprefs-groups-grid');
                                var grid = $('#prefs-groups-grid');
                                var coord = {};
                                
                                coord.top = grid.offset().top 
                                            + (grid.height() / 2);
                                coord.left = grid.offset().left 
                                            + (grid.width() / 2) 
                                            - ($dialog.width() / 2);

                                $dialog.offset(coord);
                            },
                            beforeShowForm: function(form) {
                                var colNames = this.p.colNames;
                                $.each(this.p.colModel, function (i, item) {
                                    if (item["qTip"]) {
                                        $("#tr_" + item["name"] 
                                            + " td.CaptionTD").qtip({
                                            content: {
                                                title: colNames[i],
                                                text: item["qTip"]
                                            },
                                            position: {
                                                target: "mouse",
                                                adjust: { x: 5, y: 5 },
                                                viewport: $(window)
                                            },
                                            style: "qtip-tipped"
                                        });
                                    }
                                });
                            }
                        },
                        delOptions: {
                            afterShowForm: function ($form) {
                                var $dialog = $form.closest('div.ui-jqdialog');
                                var grid = $('#prefs-groups-grid');
                                var coord = {};
                                
                                coord.top = grid.offset().top 
                                                + (grid.height() / 2);
                                coord.left = grid.offset().left 
                                                + (grid.width() / 2) 
                                                - ($dialog.width() / 2);
                                
                                $dialog.offset(coord);
                            }
                        }
                    }
                }
            ],
            rowNum: 10,
            sortname: 'name',
            autowidth: true,
            viewrecords: true,
            sortorder: 'asc',
            height: 400,
            pager: '#prefs-groups-pager'
        }).navGrid('#prefs-groups-pager', {
            edit:false,
            add:true,
            del:false,
            search:false
        }, {
            //prmEdit
            closeAfterEdit: true
        }, {
            //prmAdd,
            closeAfterAdd: true,
            beforeShowForm: function(form) {
                var colNames = this.p.colNames;
                $.each(this.p.colModel, function (i, item) {
                    if (item["qTip"]) {
                        $("#tr_" + item["name"] + " td.CaptionTD").qtip({
                            content: {
                                title: colNames[i],
                                text: item["qTip"]
                            },
                            position: {
                                target: "mouse",
                                adjust: { x: 5, y: 5 },
                                viewport: $(window)
                            },
                            style: "qtip-tipped"
                        });
                    }
                });
            },
            afterShowForm: function ($form) {
                var $dialog = $('#editmodprefs-groups-grid');
                var grid = $('#prefs-groups-grid');
                var coord = {};

                coord.top = grid.offset().top + (grid.height() / 2);
                coord.left = grid.offset().left + (grid.width() / 2) 
                                                - ($dialog.width() / 2);

                $dialog.offset(coord);
            }
        });
    }
});

/*
 * View to enable user to add/edit devices that can be used in CLIRA
 */
Clira.DevicesPrefView = Ember.View.extend({
    templateName: 'devices_pref',
    classNames: ['output-content'],

    didInsertElement: function() {
        var view = this;
        /* Set Up Devices */
        $('#prefs-devices-form').dialog({
            autoOpen: true,
            dialogClass: 'inline-dialog',
            appendTo: '#' + view.elementId,
            resizable: false,
            draggable: false,
            width: 600,
            buttons: {
                'Close': function() {
                    view.destroy();
                }
            },
            position: {
                my: 'left',
                at: 'left',
                of: '#' + view.elementId
            }
        });

        $('#' + view.elementId + ' #prefs-devices-form #prefs-devices-grid')
            .jqGrid({
            url: '/clira/system/db.slax?p=device_list',
            editurl: '/clira/system/db.slax?p=device_edit',
            datatype: 'json',
            colNames: ['Name', 'Hostname', 'Port',
                       'Username', 'Password', 'Save Password', 'Connect', ''],
            colModel: [
                {
                    name: 'name',
                    index: 'index',
                    width: 90,
                    editable: true,
                    qTip: "The name of this device to be identified by in CLIRA.",
                    editrules: {
                        required: true
                    }
                },
                {
                    name: 'hostname',
                    index: 'hostname',
                    width: 100,
                    editable: true,
                    qTip: "The host name or IP address of this device.",
                    editrules: {
                        required: true
                    }
                },
                {
                    name: 'port',
                    index: 'port',
                    width: 50,
                    editable: true,
                    qTip: "The SSH port to connect to on this device.",
                    editoptions: {
                        defaultValue: "22"
                    }
                },
                {
                    name: 'username',
                    index: 'username',
                    width: 100,
                    editable: true,
                    qTip: "The user name to connect with for this device.",
                    editrules: {
                        required: true
                    }
                },
                {
                    name: 'password',
                    index: 'password',
                    width: 40,
                    editable: true,
                    qTip: "The password to connect with for this device.  Note that if you set this here, the password will be stored in clear text in the CLIRA database file.  Please see the documentation for more information.",
                    edittype: 'password',
                    hidden: true,
                    hidedlg: true,
                    editrules: {
                        edithidden: true
                    }
                },
                {
                    name: 'save_password',
                    index: 'save_password',
                    width: 20,
                    editable: true,
                    qTip: "Save the password in CLIRA database if it has not been saved previously.",
                    edittype: 'checkbox',
                    editoptions: {
                        value: 'yes:no',
                        defaultValue: 'no'
                    },
                    formatter: 'checkbox'
                },
                {
                    name: 'connect',
                    index: 'connect',
                    width: 20,
                    editable: true,
                    qTip: "If checked, attempt to verify connectivity to the device immediately after saving the connection information.",
                    hidden: true,
                    edittype: 'checkbox',
                    editoptions: {
                        value: 'yes:no',
                        defaultValue: 'yes'
                    },
                    editrules: {
                        edithidden: true
                    },
                    formatter: 'checkbox'
                },
                {
                    name: 'action',
                    index: 'action',
                    width: 40,
                    formatter: 'actions',
                    formatoptions: {
                        editformbutton: true,
                        editOptions: {
                            closeAfterEdit: true,
                            afterShowForm: function ($form) {
                                var $dialog = $('#editmodprefs-devices-grid');
                                var grid = $('#prefs-devices-grid');
                                var coord = {};
                                
                                coord.top = grid.offset().top 
                                                + (grid.height() / 2);
                                coord.left = grid.offset().left 
                                                + (grid.width() / 2) 
                                                - ($dialog.width() / 2);
                                
                                $dialog.offset(coord);
                            },
                            beforeShowForm: function(form) {
                                $("#tr_connect", form).hide();
                                var colNames = this.p.colNames;
                                $.each(this.p.colModel, function (i, item) {
                                    if (item["qTip"]) {
                                        $("#tr_" + item["name"] 
                                                 + " td.CaptionTD").qtip({
                                            content: {
                                                title: colNames[i],
                                                text: item["qTip"]
                                            },
                                            position: {
                                                target: "mouse",
                                                adjust: { x: 5, y: 5 },
                                                viewport: $(window)
                                            },
                                            style: "qtip-tipped"
                                        });
                                    }
                                });
                            }
                        },
                        delOptions: {
                            afterShowForm: function ($form) {
                                var $dialog = $form.closest('div.ui-jqdialog');
                                var grid = $('#prefs-devices-grid');
                                var coord = {};
                                
                                coord.top = grid.offset().top + (grid.height() / 2);
                                coord.left = grid.offset().left + (grid.width() / 2) - ($dialog.width() / 2);
                                
                                $dialog.offset(coord);
                            }
                        }
                    }
                }
            ],
            rowNum: 10,
            sortname: 'name',
            autowidth: true,
            viewrecords: true,
            sortorder: 'asc',
            height: 400,
            pager: '#prefs-devices-pager'
        }).jqGrid('navGrid', '#prefs-devices-pager', {
            edit:false,
            add:true,
            del:false,
            search:false
        }, {
            //prmEdit
            closeAfterEdit: true
        }, {
            //prmAdd,
            closeAfterAdd: true,
            afterShowForm: function ($form) {
                var $dialog = $('#editmodprefs-devices-grid');
                var grid = $('#prefs-devices-grid');
                var coord = {};

                coord.top = grid.offset().top + (grid.height() / 2);
                coord.left = grid.offset().left + (grid.width() / 2) 
                                                - ($dialog.width() / 2);
                $dialog.offset(coord);
            },
            beforeShowForm: function(form) {
                var colNames = this.p.colNames;
                $.each(this.p.colModel, function (i, item) {
                    if (item["qTip"]) {
                        $("#tr_" + item["name"] + " td.CaptionTD").qtip({
                            content: {
                                title: colNames[i],
                                text: item["qTip"]
                            },
                            position: {
                                target: "mouse",
                                adjust: { x: 5, y: 5 },
                                viewport: $(window)
                            },
                            style: "qtip-tipped"
                        });
                    }
                });
            },
            afterSubmit: function(response, formdata) {
                if (formdata["connect"] == "yes") {
                    view.set('isVisible', false);
                    var cv = Ember.View.extend({
                        classNames: ['output-content'],
                        templateName: 'conn_status',
                        underlay: true,
                        didInsertElement: function() {
                            var that = this;
                            var elementId = this.elementId;
                            $("#prefs-devices-connect").dialog({
                                autoOpen: true,
                                dialogClass: 'inline-dialog',
                                height: 300,
                                appendTo: '#' + elementId,
                                width: 400,
                                resizable: false,
                                draggable: 'false',
                                modal: false,
                                position: {
                                    my: 'left',
                                    at: 'left',
                                    of: '#' + elementId
                                },
                                buttons: {
                                    'Close': function() {
                                        view.set('isVisible', true);
                                        $(this).dialog("close");
                                    }
                                },
                                close: function() {
                                },
                                open: function() {                                    
                                    var content = "<div id=\"connecting\">"
                                                + "Attempting to establish "
                                                + "connection to '"
                                                + formdata["name"] + "' (" 
                                                + formdata["username"] + "@" 
                                                + formdata["hostname"]
                                                + ":" + formdata["port"] 
                                                + ") ...</div><div class="
                                                + "\"output-replace\"></div>";
                                    var $newp = jQuery(content);
                                    $('#' + that.elementId 
                                            + ' #connect-status').empty();
                                    $('#' + that.elementId 
                                            + ' #connect-status').append($newp);

                                    var $out = $('div#' 
                                                    + that.elementId 
                                                    + ' div.output-replace');
                                    $.clira.runCommand(that, formdata["name"], 
                                                        ".noop-command", 
                                                        function (success, $output) {
                                            var msg = "<div style="
                                                    + "\"font-weight: bold\">";
                                            if (success) {
                                                msg += "Connection successful"
                                                    + ".  You can now use this"
                                                    + "device in CLIRA.";
                                            } else {
                                                msg += "Connection NOT "
                                                    + "successful.  Please "
                                                    + "check your device "
                                                    + "connection settings " 
                                                    + "and try again.";
                                            }
                                            msg += "</div>";
                                            $out.append(msg);
                                        }
                                    );
                                }
                            });
                        }
                    });
                    view.get('parentView').pushObject(cv.create());
                }
                return [ true, "" ];
            }
        });
    }
});

/*
 * View extending dynamic forms to handle displaying and saving clira
 * preferences using localStorage backed ember-restless
 */
Clira.GeneralPrefView = Clira.DynFormView.extend({
    title: "Preferences",
    buttons: [{
        caption: "Cancel",
        onclick: function() {
            this.get('parentView').destroy();
        }
    },{
        caption: "Save",
        onclick: function() {
            var clira_prefs = $.clira.prefs;
            // Iterate fieldValues and save them back into Clira.Preference
            $.each(viewContext.get('fieldValues'), function(k, v) {
                var pref = Clira.Preference.find(k);
                if (pref) {
                    pref.set('value', v);
                    pref.saveRecord();

                    // Update $.clira.pref
                    clira_prefs[k] = v;
                }
            });
            this.get('parentView').destroy();
        }
    }]
});
