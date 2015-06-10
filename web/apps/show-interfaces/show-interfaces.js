/*
 * CLIRA demo application to fetch interfaces info from a JUNOS device.
 */

$(function($) {
    $.clira.commandFile({
        name : "show-interfaces",
        templatesFile : "/apps/show-interfaces/show-interfaces.hbs",
        commands : [
            {
                command : "show interfaces",
                help : "Show interfaces on device",
                templateName : "show-interfaces",
                arguments : [
                    {
                        name : "device",
                        help : "Remote JUNOS device",
                        type : "string",
                        nokeyword : true
                    }
                ],
                execute : function (view, cmd, parse, poss) {
                    /* 
                     * Extract the device name
                     */
                    var device = poss.data.device;
                    fetchInterfaceList(view, device);
                }
            }
        ]
        
    });

    /*
     * Interface view logic to control expand and collapse of interface 
     * content.
     */
    Clira.showInterfacesApp = {
        interfaceView : Ember.View.extend({
            didInsertElement : function () {
                /*
                 * 'this' corresponds to the view context. The Jquery
                 * selector search space is restricted to the elements in the 
                 * current view's block. We save the value of 'this' to 'that'
                 * since it'll have a different meaning inside the callback.
                 */
                var that = this;
                this.$('.ifdInfo').hide();
                this.$(".ifdContainer").click(function() {
                    that.$('.ifdInfo').toggle(200);
                }); 
            }                
        })
    };

    function fetchInterfaceList (view, device) {
        var cmd = "show interfaces";
        view.set('controller.loading', true);
        $.clira.runCommand(view, device, cmd, "json", function (view, status, 
            result) {
            if (status) {
                var res =  $.parseJSON(result);
                var ifList = [];
                
                /* 
                 * Get the physical interfaces container (IFD's) 
                 */
                var ifds = res["interface-information"][0]["physical-interface"];
                
                /* 
                 * Populate our ifList array with some IFD fields 
                 */
                ifds.forEach(function (ifd) {
                    ifList.push({
                        name : field(ifd, 'name'),
                        adminStatus : field(ifd, 'admin-status'),
                        mtu : field(ifd, 'mtu'),
                        speed : field(ifd, 'speed')
                    });
                });

                /* 
                 * Update our app's view with the list of interfaces 
                 */
                view.set('controller.loading', false);
                view.set('controller.ifList', ifList);

            } else {
                /* 
                 * RPC failed to execute 
                 */
                view.set('controller.error', "RPC failed.");
            }
        });
    }
    function field(obj, prop) {
        if (obj.hasOwnProperty(prop) 
                && $.isArray(obj[prop])
                && obj[prop][0].hasOwnProperty('data')) {
            return obj[prop][0].data;
        }
        return "N/A";
    }

});
