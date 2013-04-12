<?php

/*
 * Unsure if we are going to utilize PHP in the long run, but for short term
 * testing, it is quick to deploy an interface between jQuery & sqlite3.
 */

/*
 * Temporarily, we use ~user/.juise/mixer.db as the filename.  This will change.
 *
 * This also requires the posix php module compiled in or installed.  Which 
 * stinks.
 */
$uinfo = posix_getpwuid(posix_geteuid());
$DBNAME = $uinfo['dir'] . '/.juise/mixer.db';

$db = new PDO('sqlite:' . $DBNAME);
if (!$db) {
    print json_encode(array());
}

$p = get('p');

header('Content-type: text/x-json');

$return = array();

switch ($p) {
case 'devices': $return = do_devices(); break;
case 'device_list': $return = do_device_list(); break;
case 'device_edit': $return = do_device_edit(); break;
case 'group_list': $return = do_group_list(); break;
case 'group_edit': $return = do_group_edit(); break;
case 'group_members': $return = do_group_members(); break;
}

print json_encode($return);

function
do_devices ()
{
    global $db;

    $result = $db->query('SELECT * FROM devices ORDER BY name');
    $rows = array();
    foreach ($result as $row) {
        $rows[] = array(
            'id' => $row['id'],
            'name' => $row['name'],
        );
    }

    return array('devices' => $rows);
}

/*
 * List all of the devices
 */
function
do_device_list ()
{
    global $db;

    $page = get('page');
    $limit = get('rows');
    $sidx = get('sidx');
    $sord = get('sord');
    if (!$sidx) {
        $sidx = 1;
    }
    if (!$limit) {
        $limit = 10;
    }
    if (!$page) {
        $page = 1;
    }

    $result = $db->query('SELECT COUNT(*) AS count FROM devices');
    $row = $result->fetch();

    $count = $row['count'];
    if ($count > 0) {
        $total_pages = ceil($count / $limit);
    } else {
        $total_pages = 0;
    }
    if ($page > $total_pages) {
        $page = $total_pages;
    }
    $start = $limit * $page - $limit;

    $rows = array();

    $result = $db->query("
        SELECT *
        FROM devices
        ORDER BY $sidx $sord
        LIMIT $start, $limit
    ");
    foreach ($result as $row) {
        $rows[] = array(
            'id' => $row['id'],
            'cell' => array(
                $row['name'],
                $row['hostname'],
                $row['port'],
                $row['username'],
                $row['password'] ? 'CLIR4SET' : '',
                $row['save_password'],
            )
        );
    }

    return array(
        'page' => $page,
        'total' => $total_pages,
        'records' => $count,
        'rows' => $rows,
    );
}

/*
 * Edit/Add/Delete a device
 */
function
do_device_edit ()
{
    $oper = get('oper');

    switch ($oper) {
    case 'add': return do_device_edit_add();
    case 'edit': return do_device_edit_edit();
    case 'del': return do_device_edit_del();
    }

    return array();
}

/*
 * Edit a device
 */
function
do_device_edit_edit ()
{
    global $db;

    $hostname = get('hostname');
    $id = get('id');
    $name = get('name');
    $password = get('password');
    $port = get('port');
    $save_password = get('save_password');
    $username = get('username');

    $pwchanged = ($password != 'CLIR4SET');

    if (!$id) {
        return array();
    }

    /*
     * Required: name, username, hostname.  They should be client-checked, but
     * validate anyway.
     */
    if (!$name || !$hostname || !$username) {
        return array();
    }

    $sql = 'UPDATE devices SET name = ?, hostname = ?, port = ?, username = ?, save_password = ?';
    $bind = array($name, $hostname, $port, $username, $save_password);
    if ($pwchanged) {
        $sql .= ', password = ?';
        $bind[] = $password;
    }
    $sql .= ' WHERE id = ?';
    $bind[] = $id;

    $q = $db->prepare($sql);
    $q->execute($bind);

    return array();
}

/*
 * Delete a device
 */
function
do_device_edit_del ()
{
    global $db;

    $id = get('id');

    if (!$id) {
        return array();
    }

    $q = $db->prepare('DELETE FROM devices WHERE id = ?');
    $q->execute(array($id));

    return array();
}

/*
 * Add a device
 */
function
do_device_edit_add ()
{
    global $db;

    $hostname = get('hostname');
    $name = get('name');
    $password = get('password');
    $port = get('port');
    $save_password = get('save_password');
    $username = get('username');

    /*
     * Required: name, username, hostname.  They should be client-checked, but
     * validate anyway.
     */
    if (!$name || !$hostname || !$username) {
        return array();
    }

    $q = $db->prepare('INSERT into DEVICES (name, hostname, port, username, ' .
        'password, save_password) VALUES (?, ?, ?, ?, ?, ?)');

    $q->execute(array($name, $hostname, $port, $username, $password,
        $save_password));

    return array();
}

/*
 * List all of the groups
 */
function
do_group_list ()
{
    global $db;

    $page = get('page');
    $limit = get('rows');
    $sidx = get('sidx');
    $sord = get('sord');
    if (!$sidx) {
        $sidx = 1;
    }
    if (!$limit) {
        $limit = 10;
    }
    if (!$page) {
        $page = 1;
    }

    $result = $db->query('SELECT COUNT(*) AS count FROM groups');
    $row = $result->fetch();

    $count = $row['count'];
    if ($count > 0) {
        $total_pages = ceil($count / $limit);
    } else {
        $total_pages = 0;
    }
    if ($page > $total_pages) {
        $page = $total_pages;
    }
    $start = $limit * $page - $limit;

    /*
     * Due to paging, we need to fetch the group names, then members
     */
    $result = $db->query("
        SELECT id, name
        FROM groups
        ORDER BY $sidx $sord
        LIMIT
        $start, $limit
    ");
    $rows = array();
    foreach ($result as $row) {
        $result2 = $db->query("
            SELECT devices.name
            FROM groups_members
            LEFT JOIN devices ON devices.id = groups_members.device_id
            WHERE groups_members.group_id = {$row['id']}
        ");

        $names = array();
        foreach ($result2 as $row2) {
            $names[] = $row2['name'];
        }

        $members = '';
        for ($i = 0; $i < count($names); $i++) {
            $members .= $names[$i];
            if ($i < count($names) - 1) {
                $members .= ', ';
            }
        }

        $rows[] = array(
            'id' => $row['id'],
            'cell' => array(
                $row['name'],
                $names,
            )
        );
    }

    return array(
        'page' => $page,
        'total' => $total_pages,
        'records' => $count,
        'rows' => $rows,
    );
}

/*
 * Edit/Add/Delete a group
 */
function
do_group_edit ()
{
    $oper = get('oper');

    switch ($oper) {
    case 'add': return do_group_edit_add();
    case 'edit': return do_group_edit_edit();
    case 'del': return do_group_edit_del();
    }

    return array();
}

/*
 * Edit a group
 */
function
do_group_edit_edit ()
{
    global $db;

    $id = get('id');
    $name = get('name');
    $members = preg_split('/,/', get('members'));

    if (!$id) {
        return array();
    }

    /*
     * Required: name, members.  They should be client-checked, but validate anyway.
     */
    if (!$name || !count($members)) {
        return array();
    }

    /*
     * Update the name in the groups table
     */
    $q = $db->prepare('UPDATE groups SET name = ? WHERE id = ?');
    $q->execute(array($name, $id));

    /*
     * Nuke and repopulate all the old members from the groups_members table
     */
    $q = $db->prepare('DELETE FROM groups_members WHERE group_id = ?');
    $q->execute(array($id));
    foreach ($members as $val) {
        $q = $db->prepare('
            INSERT INTO groups_members (group_id, device_id)
            VALUES (?, ?)
        ');
        $q->execute(array($id, $val));
    }

    return array();
}

/*
 * Delete a group
 */
function
do_group_edit_del ()
{
    global $db;

    $id = get('id');

    if (!$id) {
        return array();
    }

    $q = $db->prepare('DELETE FROM groups WHERE id = ?');
    $q->execute(array($id));

    return array();
}

/*
 * Add a group
 */
function
do_group_edit_add ()
{
    global $db;

    $name = get('name');
    $members = preg_split('/,/', get('members'));

    /*
     * Required: name.  They should be client-checked, but validate anyway.
     */
    if (!$name || !count($members)) {
        return array();
    }

    /*
     * Create the new group
     */
    $q = $db->prepare('INSERT INTO groups (name) VALUES(?)');
    $q->execute(array($name));

    $id = $db->lastInsertId();

    /*
     * Add the groups_members
     */
    foreach ($members as $val) {
        $q = $db->prepare('
            INSERT INTO groups_members (group_id, device_id)
            VALUES (?, ?)
        ');
        $q->execute(array($id, $val));
    }

    return array();
}

/*
 * Retrieve a group's members
 */
function
do_group_members ()
{
    global $db;

    $target = get('target');

    if (!$target) {
        return array('devices' => array($target));
    }
    
    $q = $db->prepare('
        SELECT devices.name
        FROM groups
        LEFT JOIN groups_members
            ON groups_members.group_id = groups.id
        LEFT JOIN devices
            ON devices.id = groups_members.device_id
        WHERE groups.name = ?
    ');
    $q->execute(array($target));
    $devices = array();
    while ($row = $q->fetch()) {
        $devices[] = $row['name'];
    }

    // No group members, must be a raw device name
    if (!count($devices)) {
        $devices = array($target);
    }

    return array('devices' => $devices);
}

/*
 * Sanitize request variables
 */
function
get ($key)
{
    if (isset($_REQUEST[$key])) {
        return stripslashes(trim($_REQUEST[$key]));
    }
    return null;
}

?>
