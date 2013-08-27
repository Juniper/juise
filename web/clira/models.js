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
 * Model to save command history
 */
Clira.CommandHistory = RL.Model.extend({
    command: RL.attr('string'),
    on: RL.attr('number')
});

/*
 * Redefine adapter to LocalStorage as opposed to global Client adapter
 */
Clira.CommandHistory.reopenClass({
    adapter: Ember.computed(function() {
        return RL.LSAdapter.create();
    }).property()
});
