<?php

/**
 * @file
 * Batch form and generating functions for the fw_indexsvc module.
 */

/**
 * Build the batch job.
 *
 * @param Object $ident
 *   The identifier object.
 *
 * @return Array
 *   The batch job.
 */
function fw_indexsvc_create_reindex_batch($ident) {
  $operations = array();
  $total_count = 0;
  $nodes = array();
  $files = array();
  $users = array();

  // Find all nodes for the identifier.
  if (isset($ident->sources, $ident->sources['node']) && !empty($ident->sources['node'])) {
    $query = new EntityFieldQuery();
    $result = $query->entityCondition('entity_type', 'node')
      ->entityCondition('bundle', $ident->sources['node'], 'IN')
      ->propertyCondition('status', 1)
      ->execute();
    $nodes = $result['node'];
  }

  // Find all files on nodes for the identifier.
  if (isset($ident->sources, $ident->sources['file']) && !empty($ident->sources['file'])) {
    // Get handler for files and this identifier.
    $handlers = fw_indexsvc_get_handlers('file', array($ident));
    foreach($handlers as $handler) {
      $files += $handler->get_indexed_files();
    }
  }

  // Find all users for the identifier.
  if (isset($ident->sources, $ident->sources['user']) && !empty($ident->sources['user'])) {
    // Get handler for files and this identifier.
    $handlers = fw_indexsvc_get_handlers('user', array($ident));
    foreach($handlers as $handler) {
      $users += $handler->get_indexed_users();
    }
  }

  // Calculate the total number of jobs.
  $total_count = count($nodes) + count($files) + count($users);

  if (!empty($nodes)) {
    $queue = new MemoryQueue('batch');

    // Add batch operations for all nodes.
    foreach($result['node'] as $node) {
      $queue->createItem($node->nid);
      if ($queue->numberOfItems() > NUMBER_OF_ITEMS) {
        $nodes = array();
        while ($item = $queue->claimItem()) {
          $nodes[] = $item->data;
          $queue->deleteItem($item);
        }
        $operations[] = array('fw_indexsvc_reindex_batch_operation', array($nodes, 'node', $ident, $total_count));
      }
    }
    $nodes = array();
    while ($item = $queue->claimItem()) {
      $nodes[] = $item->data;
      $queue->deleteItem($item);
    }
    $operations[] = array('fw_indexsvc_reindex_batch_operation', array($nodes, 'node', $ident, $total_count));
  }

  // Find all files on nodes for the identifier.
  if (!empty($files)) {
    $queue = new MemoryQueue('batch');

    // Add operations for each found file by the handler.
    foreach($files as $file) {
      $queue->createItem($file['fid']);
      if ($queue->numberOfItems() > NUMBER_OF_ITEMS) {
        $files = array();
        while ($item = $queue->claimItem()) {
          $files[] = $item->data;
          $queue->deleteItem($item);
        }
        $operations[] = array('fw_indexsvc_reindex_batch_operation', array($files, 'file', $ident, $total_count));
      }
    }
    $files = array();
    while ($item = $queue->claimItem()) {
      $files[] = $item->data;
      $queue->deleteItem($item);
    }
    $operations[] = array('fw_indexsvc_reindex_batch_operation', array($files, 'file', $ident, $total_count));
  }

  // Find all users for the identifier.
  if (!empty($users)) {
    $queue = new MemoryQueue('batch');

    // Add operations for each found user by the handler.
    foreach($users as $user) {
      $queue->createItem($user->uid);
      if ($queue->numberOfItems() > NUMBER_OF_ITEMS) {
        $users = array();
        while ($item = $queue->claimItem()) {
          $users[] = $item->data;
          $queue->deleteItem($item);
        }
        $operations[] = array('fw_indexsvc_reindex_batch_operation', array($users, 'user', $ident, $total_count));
      }
    }
    $users = array();
    while ($item = $queue->claimItem()) {
      $users[] = $item->data;
      $queue->deleteItem($item);
    }
    $operations[] = array('fw_indexsvc_reindex_batch_operation', array($users, 'user', $ident, $total_count));
  }

  $batch = array(
    'operations' => $operations,
    'finished' => 'fw_indexsvc_reindex_batch_finished',
  );

  return $batch;
}

/**
 * The batch process
 *
 * @param int $id
 *   The nid or fid
 * @param String $entity_type
 *   The entity type
 * @param Array $context
 *   The batch process array
 */
function fw_indexsvc_reindex_batch_operation($arr, $entity_type, $ident, $total_count = 0, &$context) {
  $old_doc_count = isset($context['results']) ? count($context['results']) : 0;
  $entities = "<ul>";

  foreach($arr as $id) {
    switch ($entity_type) {
      case 'node':
        $node = entity_load_single('node', $id);

        if ($node) {

          // Update the entity
          module_invoke('fw_indexsvc', 'entity_insert', $node, 'node');

          // Set results and messages
          $context['results'][] = array_sum(explode(' ', microtime()));
          $entities .= '<li>node/' . $node->nid . ' : ' . check_plain($node->title) . '</li>';
        }
        break;

      case 'file':
        $file = entity_load_single('file', $id);

        if ($file) {

          // Update the entity
          module_invoke('fw_indexsvc', 'entity_insert', $file, 'file');

          // Set results and messages
          $context['results'][] = array_sum(explode(' ', microtime()));
          $entities .= '<li>file/' . $file->fid . ' : ' . check_plain($file->filename) . '</li>';
        }
        break;

      case 'user':
        $user = entity_load_single('user', $id);
        if ($user) {
          // Update the entity
          module_invoke('fw_indexsvc', 'entity_insert', $user, 'user');

          // Set results and messages
          $context['results'][] = array_sum(explode(' ', microtime()));
          $entities .= '<li>users/' . $user->uid . ' : ' . check_plain($user->name) . '</li>';
        }
        break;


      default:
        break;
    }
  }

  // Store the timestamp in the context to calculate time taken.
  $doc_count = max(1, count($context['results']));
  $time_taken = $context['results'][$doc_count - 1] - $context['results'][0];
  $speed = round(($doc_count / (float)$time_taken) * 60);
  $estimated_time = ($time_taken / (float)$doc_count) * $total_count;
  $remaining_time = $estimated_time - $time_taken;
  $time_format1 = $time_taken > 3600 ? 'H:i:s' : ($time_taken > 60 ? 'i:s' : 's\s');
  $time_format2 = $remaining_time > 3600 ? 'H:i:s' : ($remaining_time > 60 ? 'i:s' : 's\s');
  $time_format3 = $estimated_time > 3600 ? 'H:i:s' : ($estimated_time > 60 ? 'i:s' : 's\s');

  // Message displayed under the progressbar.
  $right_message = '<div class="progress"><div class="percentage">@remaining remaining of @estimated</div></div>';
  $context['message'] = t($right_message . 'Processed @count/@total_count documents in @time, @speed docs/min', array(
    '@time' => ltrim(date($time_format1, strtotime(date('Y-m-d 00:00:00')) + $time_taken), '0'),
    '@speed' => $speed,
    '@count' => $doc_count,
    '@total_count' => $total_count,
    '@remaining' => ltrim(date($time_format2, strtotime(date('Y-m-d 00:00:00')) + $remaining_time), '0'),
    '@estimated' => ltrim(date($time_format3, strtotime(date('Y-m-d 00:00:00')) + $estimated_time), '0'),
  ));
  $context['message'] .=  $entities . '</ul>';
}

/**
 * Function to execute after batch process is done.
 *
 * @param bool $success
 *   True if no errors
 * @param Array $results
 *   The processed entities
 * @param Array $operations
 *   Remaining entities not processed
 */
function fw_indexsvc_reindex_batch_finished($success, $results, $operations) {

  if ($success) {
    $total_count = count($results);
    $total_time = $results[$total_count - 1] - $results[0];
    $time_format = $total_time > 3600 ? 'H:i:s' : ($total_time > 60 ? 'i:s' : 's\s');
    $speed = round(($total_count / (float)$total_time) * 60);

    // Here we could do something meaningful with the results.
    // We just display the number of entities we processed...
    drupal_set_message(t('@count entities processed in @time_taken, @speed docs/min.', array(
      '@count' => $total_count,
      '@time_taken' => date($time_format, strtotime(date('Y-m-d 00:00:00')) + $total_time),
      '@speed' => $speed,
    )));
  } else {
    // An error occurred.
    // $operations contains the operations that remained unprocessed.
    $error_operation = reset($operations);
    drupal_set_message(t('An error occurred while processing @operation with arguments : @args', array('@operation' => $error_operation[0], '@args' => print_r($error_operation[0], TRUE))));
  }
}
