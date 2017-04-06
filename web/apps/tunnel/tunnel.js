/*
 * Copyright 2015, Juniper Network Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

jQuery.clira.commandFile({
    name: "ipsec-tunnel",
    commands: [
        {
            command: "build ipsec tunnel",
            help: "Build an IPsec tunnel on target device",
            arguments: [{
                name: "device1",
                type: "string",
                help: "One end of IPsec tunnel",
                nokeyword: true,
                mandatory: true
            }, {
                name: "device2",
                type: "string",
                help: "Other end of IPsec tunnel",
                nokeyword: true,
                mandatory: true
           
            }],

            execute: function(view, cmd, parse, poss) {
                var device1 = poss.data.device1,
                    device2 = poss.data.device2;

                if (!device1 || !device2) {
                    $.clira.makeAlert(view, "You must include both "
                        + "ends to build IPsec tunnel");
                    return;
                }
                collectFormFields(view, device1, device2);
            }
        }
    ]
});

function collectFormFields(view, device1, device2) {
    var fields = [{
            name: 'device1_details',
            title: device1 + ' details',
            spacer: true,
            header: true
        }, {
            name: 'device1_ifname',
            title: 'Interface name',
            help: 'Interface to be used on ' + device1,
            mandatory: true,
            match: '^(si-)',
            matchMessage: 'Must be a services interface',
            fieldType: 'TYPE_STRING'
        }, {
            name: 'device1_ifunit',
            title: 'Interface unit',
            help: 'Unit number for interface on ' + device1,
            mandatory: true,
            fieldType: 'TYPE_UINT'
        }, {
            name: 'device1_ifip',
            title: 'IP address',
            help: 'IP address for this interface on ' + device1,
            mandatory: true,
            fieldType: 'TYPE_STRING'
        }, {
            name: 'device1_nhop',
            title: 'Next hop IP',
            help: 'IP address of next hop for ' + device1,
            mandatory: true,
            fieldType: 'TYPE_STRING'
        }, {
            name: 'device1_inif',
            title: 'Inside interface',
            help: 'Interface inside to network for ' + device1,
            mandatory: true,
            fieldType: 'TYPE_STRING'
        }, {
            name: 'device1_inifunit',
            title: 'Inside interface unit',
            help: 'Interface unit inside to network for ' + device1,
            mandatory: true,
            fieldType: 'TYPE_STRING'
        }, {
            name: 'device1_inifip',
            title: 'Inside interface address',
            help: 'Interface address inside to network for ' + device1,
            mandatory: true,
            fieldType: 'TYPE_STRING'
        }, {
            spacer: true
        }, {
            name: 'device2_details',
            title: device2 + ' details',
            spacer: true,
            header: true,
        }, {
            name: 'device2_ifname',
            title: 'Interface name',
            help: 'Interface to be used on ' + device2,
            mandatory: true,
            match: '^(si-)',
            matchMessage: 'Must be a services interface',
            fieldType: 'TYPE_STRING'
        }, {
            name: 'device2_ifunit',
            title: 'Interface unit',
            help: 'Unit number for interface on ' + device2,
            mandatory: true,
            fieldType: 'TYPE_UINT'
        }, {
            name: 'device2_ifip',
            title: 'IP address',
            help: 'IP address for this interface on ' + device2,
            mandatory: true,
            fieldType: 'TYPE_STRING'
        }, {
            name: 'device2_nhop',
            title: 'Next hop IP',
            help: 'IP address of next hop for ' + device2,
            mandatory: true,
            fieldType: 'TYPE_STRING'
        }, {
            name: 'device2_inif',
            title: 'Inside interface',
            help: 'Interface inside to network for ' + device2,
            mandatory: true,
            fieldType: 'TYPE_STRING'
        }, {
            name: 'device2_inifunit',
            title: 'Inside interface unit',
            help: 'Interface unit inside to network for ' + device2,
            mandatory: true,
            fieldType: 'TYPE_STRING'
        }, {
            name: 'device2_inifip',
            title: 'Inside interface address',
            help: 'Interface address inside to network for ' + device2,
            mandatory: true,
            fieldType: 'TYPE_STRING'
        }, {
            spacer: true
        }, {
            name: 'ike_header',
            title: 'Phase-1 (IKE)',
            spacer: true,
            header: true
        }, {
            name: 'ike_ascii_key',
            title: 'Pre shared ASCII key',
            fieldType: 'TYPE_STRING',
            mandatory: true,
            help: 'ASCII key that is preshared between both the devices'
        }, {
            name: 'ike_auth_algo',
            title: 'Authentication Algorithm',
            fieldType: 'TYPE_STRING',
            help: 'Authentication algorithm to be used for the tunnel',
            select: true,
            filter: false,
            mandatory: true,
            value: 'sha1',
            data: [ 'md5', 'sha-256', 'sha-384', 'sha1' ]
        }, {
            name: 'ike_enc_algo',
            title: 'Encryption Algorithm',
            fieldType: 'TYPE_STRING',
            help: 'Authentication algorithm to be used for the tunnel',
            select: true,
            filter: false,
            mandatory: true,
            value: '3des-cbc',
            data: [ '3des-cbc', 'aes-128-cbc', 'aes-192-cbc', 'aes-256-cbc',
                    'des-cbc' ]
        }, {
            name: 'ike_dh_group',
            title: 'Diffie-Hellman group',
            fieldType: 'TYPE_STRING',
            help: 'Diffie-Hellman group',
            select: true,
            filter: false,
            mandatory: true,
            value: 'group5',
            data: [ 'group1',  'group14',  'group19',  'group2',  'group20',
                    'group5' ]
        }, {
            name: 'ike_lifetime',
            title: 'Lifetime (sec)',
            fieldType: 'TYPE_UINT',
            help: 'Lifetime, in seconds (180..86400 seconds)',
            value: '28800',
            mandatory: true,
            rangeMin: 180,
            rangeMax: 86400
        }, {
            name: 'ike_nat_keepalive',
            title: 'NAT keepalive',
            fieldType: 'TYPE_UINT',
            mandatory: true,
            help: 'Interval at which to send NAT keepalives (1..300 seconds)',
            value: 10,
            rangeMin: 1,
            rangeMax: 300
        }, {
            spacer: true
        }, {
            name: 'ipsec_header',
            title: 'Phase-2 (IPsec)',
            spacer: true,
            header: true
        }, {
            name: 'ipsec_protocol',
            title: 'IPsec protocol',
            fieldType: 'TYPE_STRING',
            help: 'Protocol for IPsec proposal',
            select: true,
            filter: false,
            mandatory: true,
            value: 'esp',
            data: [ 'ah', 'bundle', 'esp' ]
        }, {
            name: 'ipsec_auth_algo',
            title: 'Authentication Algorithm',
            fieldType: 'TYPE_STRING',
            help: 'Authentication algorithm to be used for IPsec',
            select: true,
            filter: false,
            mandatory: true,
            value: 'hmac-sha1-96',
            data: [ 'hmac-md5-96', 'hmac-sha1-96' ]
        }, {
            name: 'ipsec_enc_algo',
            title: 'Encryption Algorithm',
            fieldType: 'TYPE_STRING',
            help: 'Authentication algorithm to be used for IPsec',
            select: true,
            filter: false,
            mandatory: true,
            value: '3des-cbc',
            data: [ '3des-cbc', 'aes-128-cbc', 'aes-192-cbc', 'aes-256-cbc',
                    'des-cbc' ]
        }, {
            name: 'ipsec_pfs_keys',
            title: 'PFS key',
            fieldType: 'TYPE_STRING',
            help: 'Perfect forward secrecy key',
            select: true,
            filter: false,
            mandatory: true,
            value: 'group5',
            data: [ 'group1', 'group14', 'group19', 'group2', 'group20',
                    'group5' ]
        }, {
            name: 'ipsec_lifetime',
            title: 'Lifetime (sec)',
            fieldType: 'TYPE_UINT',
            help: 'Lifetime, in seconds (180..86400 seconds)',
            mandatory: true,
            value: 86400,
            rangeMin: 180,
            rangeMax: 86400
        }, {
            name: 'device1',
            hidden: true,
            value: device1
        }, {
            name: 'device2',
            hidden: true,
            value: device2
        }
    ];

    var buttons = [
        {
            caption: 'Create',
            onclick: function() {
                var v = Ember.View.create({ 
                    controller: new Ember.Controller(),
                    templateName: 'output_content'
                });
                view.get('parentView').pushObject(v);
                createTunnel(v, view.get('controller.fieldValues'));
            }
        }
    ];

    $.clira.buildForm(view, fields, buttons, 'Tunnel between ' 
                                             + device1 + ', ' + device2);
}

function createTunnel(view, values) {
    $.clira.runSlax({
        script: '/apps/tunnel/tunnel.slax',
        args: {
            device1: values['device1'],
            device2: values['device2'],
            device1_ifname: values['device1_ifname'],
            device1_ifunit: values['device1_ifunit'],
            device1_ifnameunit: values['device1_ifname']
                                + '.' + values['device1_ifunit'],
            device1_ifip: values['device1_ifip'],
            device1_nhop: values['device1_nhop'],
            device1_inif: values['device1_inif'],
            device1_inifunit: values['device1_inifunit'],
            device1_inifip: values['device1_inifip'],
            device2_ifname: values['device2_ifname'],
            device2_ifunit: values['device2_ifunit'],
            device2_ifnameunit: values['device2_ifname']
                                + '.' + values['device2_ifunit'],
            device2_ifip: values['device2_ifip'],
            device2_nhop: values['device2_nhop'],
            device2_inif: values['device2_inif'],
            device2_inifunit: values['device2_inifunit'],
            device2_inifip: values['device2_inifip'],
            ike_ascii_key: values['ike_ascii_key'],
            ike_auth_algo: values['ike_auth_algo'],
            ike_dh_group: values['ike_dh_group'],
            ike_enc_algo: values['ike_enc_algo'],
            ike_gateway_address1: values['ike_gateway_address1'],
            ike_gateway_address2: values['ike_gateway_adderss2'],
            ike_lifetime: values['ike_lifetime'],
            ike_nat_keepalive: values['ike_nat_keepalive'],
            ipsec_auth_algo: values['ipsec_auth_algo'],
            ipsec_enc_algo: values['ipsec_enc_algo'],
            ipsec_lifetime: values['ipsec_lifetime'],
            ipsec_pfs_keys: values['ipsec_pfs_keys'],
            ipsec_protocol: values['ipsec_protocol']            
        },
        view: view,
        success: function(data) {
            view.get('controller').set('output', data);
        },
        failure: function(data) {
            $.clira.makeAlert(view, "Failed to create tunnel: " + data);
        }
    });
}
