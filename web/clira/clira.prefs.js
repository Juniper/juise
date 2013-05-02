/*
 * $Id$
 *  -*-  indent-tabs-mode:nil -*-
 * Copyright 2011, Juniper Network Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

jQuery(function ($) {
    if ($.clira == undefined)
        $.clira = { };

    $.extend($.clira, {
        prefs: { },
        prefsInit: function prefsInit () {
            $.clira.prefs_form = $.yform(prefs_options, "#prefs-dialog",
                                         prefs_fields);
            $.clira.prefs = $.clira.prefs_form.getData();
            buildForms();
        },
    });

    function buildForm () {
        $("#prefs").button().click(function (event) {
            $.dbgpr("prefsEdit:", event.type);
            if ($.clira.prefs_form.shown()) {
                $.clira.prefs_form.close();

                /* Put the focus back where it belongs */
                $.clira.refocus();

            } else {
                $.clira.prefs_form.open();
            }
        });

        $("#prefs-main-form").dialog({
            autoOpen: false,
            height: 300,
            width: 350,
            modal: true,
            buttons: {
                'Close': function() {
                    $(this).dialog("close");
                }
            },
            close: function() {
            },
        });

        $("#prefsbtn").click(function() {
            $("#prefs-main-form").dialog("open");
        });

        /* Set Up Devices */
        $("#prefs-devices-form").dialog({
            autoOpen: false,
            height: 600,
            width: 800,
            resizable: false,
            modal: true,
            buttons: {
                'Close': function() {
                    $(this).dialog("close");
                }
            },
            close: function() {
            },
        });

        $("#prefs-devices").click(function() {
            $("#prefs-devices-form").dialog("open");

            $("#prefs-devices-grid").jqGrid({
                url: '/clira/db.php?p=device_list',
                editurl: '/clira/db.php?p=device_edit',
                datatype: 'json',
                colNames: ['Name', 'Hostname', 'Port',
                           'Username', 'Password', 'Save Password', ''],
                colModel: [
                    {
                        name: 'name',
                        index: 'index',
                        width: 90,
                        editable: true,
                        editrules: {
                            required: true
                        },
                    },
                    {
                        name: 'hostname',
                        index: 'hostname',
                        width: 100,
                        editable: true,
                        editrules: {
                            required: true
                        },
                    },
                    {
                        name: 'port',
                        index: 'port',
                        width: 50,
                        editable: true
                    },
                    {
                        name: 'username',
                        index: 'username',
                        width: 100,
                        editable: true,
                        editrules: {
                            required: true
                        },
                    },
                    {
                        name: 'password',
                        index: 'password',
                        width: 40,
                        editable: true,
                        edittype: 'password',
                        hidden: true,
                        hidedlg: true,
                        editrules: {
                            edithidden: true,
                        },
                    },
                    {
                        name: 'save_password',
                        index: 'save_password',
                        width: 20,
                        editable: true,
                        edittype: 'checkbox',
                        editoptions: {
                            value: 'yes:no',
                            defaultValue: 'no',
                        },
                        formatter: 'checkbox',
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
                                    
                                    coord.top = grid.offset().top + (grid.height() / 2);
                                    coord.left = grid.offset().left + (grid.width() / 2) - ($dialog.width() / 2);
                                    
                                    $dialog.offset(coord);
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
                            },
                        },
                    },
                ],
                rowNum: 10,
                sortname: 'name',
                autowidth: true,
                viewrecords: true,
                sortorder: 'asc',
                height: 400,
                pager: '#prefs-devices-pager',
                beforeSelectRow: function (rowid, e) {
                    if (e.target.id == 'delete') {
                        // Do our row delete
                        alert('delete row ' + rowid);
                    }
                },
            }).navGrid('#prefs-devices-pager', {
                edit:false,
                add:true,
                del:false,
                search:false,
            }, {
                //prmEdit
                closeAfterEdit: true
            }, {
                //prmAdd,
                closeAfterAdd: true
            });
        });
        
        /* Set Up Groups */
        $("#prefs-groups-form").dialog({
            autoOpen: false,
            height: 600,
            width: 800,
            resizable: false,
            modal: true,
            buttons: {
                'Close': function() {
                    $(this).dialog("close");
                }
            },
            close: function() {
            }
        });

        $("#prefs-groups").click(function() {
            $("#prefs-groups-form").dialog("open");
            
            $("#prefs-groups-grid").jqGrid({
                url: '/clira/db.php?p=group_list',
                editurl: '/clira/db.php?p=group_edit',
                datatype: 'json',
                colNames: ['Name', 'Members', ''],
                colModel: [
                    {
                        name: 'name',
                        index: 'name',
                        width: 90,
                        editable: true,
                        editrules: {
                            required: true
                        },
                    },
                    {
                        name: 'members',
                        index: 'devices',
                        editable: true,
                        edittype: 'select',
                        editrules: {
                            required: true,
                        },
                        editoptions: {
                            multiple: true,
                            dataUrl: '/clira/db.php?p=device',
                            buildSelect: function (data) {
                                var j = $.parseJSON(data);
                                var s = '<select>';
                                if (j.devices && j.devices.length) {
                                    $.each(j.devices, function (i, item) {
                                        s += '<option value="' + item.id + '">' + item.name + '</option>';
                                    });
                                }
                                return $(s)[0];
                            },
                        },
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
                                    
                                    coord.top = grid.offset().top + (grid.height() / 2);
                                    coord.left = grid.offset().left + (grid.width() / 2) - ($dialog.width() / 2);

                                    $dialog.offset(coord);
                                }
                            },
                            delOptions: {
                                afterShowForm: function ($form) {
                                    var $dialog = $form.closest('div.ui-jqdialog');
                                    var grid = $('#prefs-groups-grid');
                                    var coord = {};
                                    
                                    coord.top = grid.offset().top + (grid.height() / 2);
                                    coord.left = grid.offset().left + (grid.width() / 2) - ($dialog.width() / 2);
                                    
                                    $dialog.offset(coord);
                                }
                            },
                        },
                    },
                ],
                rowNum: 10,
                sortname: 'name',
                autowidth: true,
                viewrecords: true,
                sortorder: 'asc',
                height: 400,
                pager: '#prefs-groups-pager',
                beforeSelectRow: function (rowid, e) {
                    if (e.target.id == 'delete') {
                        // Do our row delete
                        alert('delete row ' + rowid);
                    }
                },
            }).navGrid('#prefs-groups-pager', {
                edit:false,
                add:true,
                del:false,
                search:false,
            }, {
                //prmEdit
                closeAfterEdit: true
            }, {
                //prmAdd,
                closeAfterAdd: true
            });
        });

        $.extend(jQuery.jgrid.edit, { recreateForm: true });
       
        /* General Preferences */
        $("#prefs-general").click(function() {
            //$("#prefs-general-form").dialog("open");
            // XXX: rkj    use new forms, decide what prefs we need?
            if ($.clira.prefs_form.shown()) {
                $.clira.prefs_form.close();
                    
                if (tgtHistory.value())
                    cmdHistory.focus();
                else tgtHistory.focus();

            } else {
                $.clira.prefs_form.open();
            }
        });
        
        $("#prefs-general-form").dialog({
            autoOpen: false,
            height: 600,
            width: 800,
            resizable: false,
            modal: true,
            buttons: {
                'Close': function() {
                    $(this).dialog("close");
                }
            },
            close: function() {
            }
        });
    }
});
