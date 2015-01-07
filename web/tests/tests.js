module('Integration tests', {
    setup: function() {
    }
});

test( "Welcome is the first command", function() {
    expect(1);

    visit('/').then(function() {
        var cName = $(find('div.output-header').find('b').toArray()[0]);
        equal(cName.text(), 'Welcome');
    });
});

test( "Execute show commands", function() {
    expect(1);

    visit('/').then(function() {
        executeCommand('show commands');

        andThen(function() {
            var cName = $(find('div.output-header').find('b').toArray()[0]);
            equal(cName.text(), 'show commands');
        });
    });
});

test( "show command history", function() {
    expect(2);

    visit('/').then(function() {
        executeCommand('show command history');

        andThen(function() {
            var cName = $(find('div.output-header').find('b').toArray()[0]);
            equal(cName.text(), 'show command history');
            
            var lastCmd = find('div.history-element').text()
                                                     .split('-')[0].trim();
            equal(lastCmd, 'show command history');
        });
    });
});

test( "clear command history", function() {
    expect(1);

    visit('/').then(function() {
        executeCommand('clear command history');

        andThen(function() {
            var op = $(find('div.output-content').toArray()[0]);
            equal(op.text().trim(), 'Successfully cleared history');
            
        });
    });
});

test( "check command history dropdown", function() {
    expect(1);

    visit('/').then(function() {
        click('button.ui-button');
        andThen(function() {
            var op = $(find('div.mru-item').toArray()[0]);
            equal(op.text().trim(), 'clear command history');
        });
    });
});

test( "Click on prefs button", function() {
    expect(1);

    visit('/').then(function() {
        click('div.prefsbtn');
        andThen(function() {
            var cName = $(find('div.output-header').find('b').toArray()[0]);
            equal(cName.text(), 'edit preferences');
        });
    });
});
