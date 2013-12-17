/*
 * $Id$
 *  -*-  indent-tabs-mode:nil -*-
 * Copyright 2013, Juniper Networks Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */


/*
 * Use localStorage adapter to store our records
 */
Clira.LSAdapter = RL.LSAdapter.create();

Clira.Client = RL.Client.create({
    adapter: Clira.LSAdapter
});

/*
 * Use 'name' as primary key in preference model
 */
Clira.LSAdapter.map("Clira.Preference", {
    primaryKey: "name"
});

/*
 * Model to save command history
 */
Clira.CommandHistory = RL.Model.extend({
    command: RL.attr('string'),
    on: RL.attr('number')
});

/*
 * Model to save Clira preferencs
 */
Clira.Preference = RL.Model.extend({
    change: RL.attr('string'),
    def: RL.attr('string'),
    label: RL.attr('string'),
    name: RL.attr('string'),
    title: RL.attr('string'),
    type: RL.attr('string'),
    value: RL.attr('string')
});
