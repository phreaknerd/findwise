<?php

/**
 * Expire queue implementation.
 *
 * This adds a method where one can count old items in the queue.
 */
class ExpireQueue extends SystemQueue {

  /**
   * Count of expired items.
   *
   * @param $expire Integer Seconds since now (defaults to 1 seconds).
   *
   * @return Integer
   */
  public function expired($expire = 1) {
    return db_query('SELECT COUNT(item_id) FROM {queue} WHERE created <= :expired AND name = :name ORDER BY created DESC LIMIT 1', array(
      ':expired' => strtotime('now -' . $expire . ' sec'),
      ':name' => $this->name
    ))->fetchField();
  }
}
