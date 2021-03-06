<?php

/**
 * @file
 * Handles the generation and validation of authentication tokens.
 *
 * These are to be used when dealing with applications such as Djatoka that do
 *  not pass through credentials.
 */

// Token lifespan(seconds): after this duration the token expires.
// 5 minutes.
define('ISLANDORA_AUTHTOKEN_TOKEN_TIMEOUT', 300);

/**
 * Request Islandora to construct an object/datastream authentication token.
 *
 * This token can later be turned in for access to the requested object or
 * datastream.
 *
 * @param string $pid
 *   The Fedora PID to generate the token for.
 * @param string $dsid
 *   The Fedora datastream ID to generate the token for.
 * @param int $uses
 *   Defaults to 1.
 *   The number of uses the token should be used for.  There are
 *   times when this should be greater than 1: ie. Djatoka needs
 *   to make two calls. This is the number of times it can be called
 *   from different php sessions, not in the run of a program. (it is
 *   statically cached).
 *
 * @return string
 *   The generated authentication token.
 */
function islandora_get_object_token($pid, $dsid, $uses = 1) {
  global $user;
  $time = time();
  $token = bin2hex(drupal_random_bytes(32));
  $id = db_insert("islandora_authtokens")->fields(
    array(
      'token' => $token,
      'uid' => $user->uid,
      'pid' => $pid,
      'dsid' => $dsid,
      'time' => $time,
      'remaining_uses' => $uses,
    ))->execute();

  return $token;
}

/**
 * Submit a token to islandora for authentication.
 *
 * Supply islandora with the token and the object/datastream it is for and you
 * will receive access if authentication passes. Tokens can only be redeemed
 * in a short window after their creation.
 *
 * @param string $pid
 *   The PID of the object to retrieve.
 * @param string $dsid
 *   The datastream id to retrieve.
 * @param string $token
 *   The registered token that allows access to this object.
 *
 * @return mixed
 *   The user credentials for access if the token validation passes,
 *   FALSE otherwise
 */
function islandora_validate_object_token($pid, $dsid, $token) {
  static $accounts = array();

  if (!empty($accounts[$pid][$dsid][$token])) {
    return $accounts[$pid][$dsid][$token];
  }

  // Check for database token.
  $time = time();
  $query = db_select('islandora_authtokens', 'tokens');
  $query->join('users', 'u', 'tokens.uid = u.uid');
  // The results will look like user objects.
  $result = $query
    ->fields('u', array('uid', 'name', 'pass'))
    ->fields('tokens', array('remaining_uses'))
    ->condition('token', $token, '=')
    ->condition('pid', $pid, '=')
    ->condition('dsid', $dsid, '=')
    ->condition('time', $time, '<=')
    ->condition('time', $time - ISLANDORA_AUTHTOKEN_TOKEN_TIMEOUT, '>')
    ->execute()
    ->fetchAll();
  if ($result) {
    $remaining_uses = $result[0]->remaining_uses;
    $remaining_uses--;
    // Remove the authentication token so it can't be used again.
    if ($remaining_uses == 0) {
      db_delete("islandora_authtokens")
        ->condition('token', $token, '=')
        ->condition('pid', $pid, '=')
        ->condition('dsid', $dsid, '=')
        ->execute();
    }
    // Decrement authentication token uses.
    else {
      db_update("islandora_authtokens")
      ->fields(array('remaining_uses' => $remaining_uses))
      ->condition('token', $token, '=')
      ->condition('pid', $pid, '=')
      ->condition('dsid', $dsid, '=')
      ->execute();
    }
    unset($result[0]->remaining_uses);
    $accounts[$pid][$dsid][$token] = $result[0];
  }
  else {
    $accounts[$pid][$dsid][$token] = FALSE;
  }

  return $accounts[$pid][$dsid][$token];
}

/**
 * Will remove any expired authentication tokens.
 */
function islandora_remove_expired_tokens() {
  $time = time();
  db_delete("islandora_authtokens")
    ->condition('time', $time - ISLANDORA_AUTHTOKEN_TOKEN_TIMEOUT, '<')
    ->execute();
}
