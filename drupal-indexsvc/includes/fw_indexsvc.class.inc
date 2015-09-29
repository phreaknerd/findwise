<?php

define('FW_FW_NUMBER_OF_ITEMS', 30);

class fwIndexSvc {

  private $name;
  private $endpoint;
  private $sources;
  private $settings;

  public function __construct($ident) {
    $this->name = $ident->name;
    $this->sources = $ident->sources;
    $this->settings = $ident->settings;
    $this->set_endpoint($ident->endpoint);
  }

  /**
   * Get name of this handler (which is the name of the ident).
   *
   * @return String
   */
  public function __toString() {
    return $this->name;
  }

  /**
   * Set the sources for this handler.
   *
   * @param Array An array of sources for this handler.
   *
   * @return
   */
  public function set_sources($value) {
    $this->sources = $value;
  }

  /**
   * All sources for this handler.
   *
   * @return Array All sources for this handler.
   */
  public function get_sources() {
    return $this->sources;
  }

  /**
   * All sources for this handler.
   *
   * @return Array All sources for this handler.
   */
  public function get_settings() {
    return $this->settings;
  }

  /**
   * Set the endpoint URL.
   *
   * @param String $uri The full URL to the endpoint.
   *
   * @return
   */
  public function set_endpoint($uri) {
    // Check URI and strip ending slash.
    $this->endpoint = preg_replace('/(\/)+$/', '', check_url($uri));
  }

  /**
   * Get endpoint URL for this handler.
   *
   * @return String Endpoint
   */
  public function get_endpoint() {
    return $this->endpoint;
  }

  /**
   * Get REST endpoint URL for this handler.
   *
   * @return String REST endpoint
   */
  public function get_rest_endpoint() {
    return $this->endpoint . '/rest/' . check_plain($this->name);
  }

  /**
   * Helper function to format the document into a XML string.
   *
   * @param Array $document
   *   The document
   * @return String XML
   */
  private static function xml($document) {
    $xml = '<doc id="' . $document['docId']['content'] . '">';
    foreach ($document as $field) {
      $xml .= '<field type="' . $field['type'] . '" name="' .
        $field['name'] . '">' . $field['content'] . '</field>';
    }
    $xml .= '</doc>';
    return $xml;
  }

  /**
   * Helper function to format the document into a JSON string.
   *
   * @param Array $document
   *   The document
   * @return String XML
   */
  private static function json($document) {
    return json_encode(array_values($document));
  }

  /**
   * Count total amount of entities in the database chosen to be indexed.
   *
   * @param Array $result The result of a call to the 'summery' on the service.
   *   If left empty a new call will be done.
   *
   * @return Integer Total count.
   */
  public function count($result = NULL) {
    $local_count = 0;
    $remote_count = 0;

    // If result is not passed in to this function, grab a new summary.
    if (is_null($result)) {
      $result = $this->GET('summary.xml');
    }

    // Summarize the count for the remote service.
    if ($result->code == 200 && isset($result->data)) {
      $xml = new SimpleXMLElement($result->data);
      // Summarize counts for all except those removed or issued removed.
      $search = $xml->xpath("/identSummary/statusCountMap/entry[@key!='removed'
        and @key!='removedissued' and @key!='recieved']");
      while (list(, $node) = each($search)) {
        $remote_count += (int) $node;
      }
    } else {
      $remote_count = '-';
    }

    // Summarize the count for the local storages.
    if (isset($this->sources)) {
      // Count local entities.
      $empty_options = TRUE;
      foreach ($this->sources as $entity_type => $options) {
        if (!empty($options)) {
          $empty_options = FALSE;
          if (method_exists($this, ($method_name = "count_local_{$entity_type}"))) {
            $local_count += call_user_func(array(&$this, $method_name), $options);
          }
        }
      }
      if ($empty_options) {
        $local_count = '-';
      }
    }

    return (object) array(
        'local' => $local_count,
        'remote' => $remote_count,
    );
  }

  /**
   * Count amount of nodes in the database.
   *
   * @param Array $bundles The node types.
   *
   * @return Integer Total count of nodes.
   */
  private function count_local_node($bundles) {
    if (empty($bundles)) {
      return 0;
    }
    $query = new EntityFieldQuery();
    $query->entityCondition('entity_type', 'node')
      ->entityCondition('bundle', $bundles, 'IN')
      ->propertyCondition('status', 1);
    // Allow other modules to alter the query.
    drupal_alter('indexed_nodes', $query, $this);
    return $query->count()->execute();
  }

  /**
   * Count amount of files in the database.
   *
   * @param Array $field_names The filefield names.
   *
   * @return Integer Total count of files.
   */
  private function count_local_file($field_names) {
    $query = $this->get_local_file_query($field_names);
    $query->addExpression('COUNT(DISTINCT fid)', 'file_count');
    $result = $query->execute()->fetchAssoc();
    return $result['file_count'];
  }

  /**
   * Count amount of users in the database.
   *
   * @param Array $roles The user-roles.
   *
   * @return Integer Total count of users.
   */
  private function count_local_user($roles) {
    if (empty($roles)) {
      return 0;
    }
    $query = $this->get_local_user_query($roles);
    $query->addExpression('COUNT(DISTINCT uid)', 'user_count');
    $result = $query->execute()->fetchAssoc();
    return $result['user_count'];
  }

  /**
   * Get the indexed files.
   *
   * @return Array
   */
  public function get_indexed_files() {
    $files = array();
    if (isset($this->sources['file']) && !empty($this->sources['file'])) {
      $query = $this->get_local_file_query($this->sources['file']);
      $query->fields('fm', array('fid'));
      if ($query->preExecute()) {
        $result = $query->execute();
        while($record = $result->fetchAssoc()) {
          $files[] = $record;
        }
      }
      else {
        watchdog('fw_indexsvc', 'Something is wrong with the query for the files.');
      }
    }
    return $files;
  }

  /**
   * Get the indexed users.
   *
   * @return Array
   */
  public function get_indexed_users() {
    $users = array();
    if (isset($this->sources['user']) && !empty($this->sources['user'])) {
      $query = $this->get_local_user_query($this->sources['user']);
      $query->fields('ur', array('uid'));
      $query->distinct();
      if ($query->preExecute()) {
        $result = $query->execute();
        $users = $result->fetchCol();
        if(count($users)) {
          $users = user_load_multiple($users);
        }
        foreach($users as &$user) {
          $user->type = 'user';
        }
      }
      else {
        watchdog('fw_indexsvc', 'Something is wrong with the query for the users.');
      }
    }
    return $users;
  }

  /**
   * Get a SelectQuery object with all indexed files in the database.
   *
   * @param Array $field_names The filefield names.
   *
   * @return SelectQuery
   */
  private function get_local_file_query($field_names) {
    $query = db_select('file_managed', 'fm');
    $query->condition('fm.status', 1);

    if (!in_array('all', $field_names)) {
      $or = db_or();
      foreach ($field_names as $field_name) {
        // Create a abbriviation for the field table.
        $table_abbr = preg_replace('/([a-z])[a-z]*(?:_|$)/i', '$1', $field_name);
        // Join together the results from multiple fields.
        $query->leftjoin("field_data_{$field_name}", $table_abbr, "fm.fid = {$table_abbr}.{$field_name}_fid");
        $or->isNotNull("{$table_abbr}.{$field_name}_fid");
      }
      $query->condition($or);
    }
    // Allow other modules to alter the query.
    drupal_alter('indexed_files', $query, $this);
    return $query;
  }

  /**
   * Get a SelectQuery object with all users by role in the database.
   *
   * @param Array $rids The role ids.
   *
   * @return SelectQuery
   */
  private function get_local_user_query($rids) 
  {
    $query = db_select('users_roles', 'ur');
    $query->condition('ur.rid', $rids, 'IN');

    // Allow other modules to alter the query.
    drupal_alter('indexed_users', $query, $this);
    return $query;
  }

  /**
   * Dispatcher for all bundle types to insert and update the search index.
   *
   * @param Object $entity
   *   The entity object.
   *
   * @return Array Result.
   */
  public function insert($entity, $type) {
    if ($this->is_indexed($entity, $type)) {
      // Build fields for the entity.
      $fields = $this->build_fields($entity, $type);

      // Structure the data.
      $data = $this::xml($fields);

      // Send the http request.
      return $this->POST('documents', NULL, $data);
    }
  }

  /**
   * Dispatcher for all bundle types to insert and update the search index.
   *
   * @param Object $entity
   *   The entity object.
   *
   * @return Array Result.
   */
  public function update($entity, $type) {
    if ($this->is_indexed($entity, $type)) {

      // Build fields for the entity.
      $fields = $this->build_fields($entity, $type);

      // Structure the data.
      $data = $this::xml($fields);

      // Send the http request.
      return $this->PUT('documents', NULL, $data);
    }
  }

  /**
   * Dispatcher for all bundle types to delete in the search index.
   *
   * @param Object $entity
   *   The entity object.
   *
   * @return Array Result.
   */
  public function remove($entity, $type) {
    $docId = $this->get_docId($entity, $type);
    return $this->DELETE('documents', array('ids' => $docId));
  }

  /**
   * Set request headers.
   *
   * @return Array
   */
  private function default_headers() {
    return array(
      'Content-Type' => 'application/xml',
      'charset' => 'UTF-8',
    );
  }

  /**
   * Clear the index from all jobs.
   *
   * @return Boolean
   */
  public function clear() {
    $this->DELETE('documents', array('status' => 'received'));
    $this->DELETE('documents', array('status' => 'committed'));
    $this->DELETE('documents', array('status' => 'indexissued'));
    $this->DELETE('documents', array('status' => 'indexed'));
    $this->DELETE('documents', array('status' => 'serviceerror'));
    $this->DELETE('documents', array('status' => 'inputerror'));
    $this->DELETE('documents', array('status' => 'indexingerror'));
    return true;
  }

  /**
   * GET HTTP request to the service.
   *
   * @param string $method
   *   The request method.
   * @param Array $query
   *   The query items.
   * @param Array $headers
   *   The header items.
   *
   * @return Object http_request
   */
  private function GET($method, $query = NULL, $headers = array()) {
    return $this->send('GET', $method, $query, NULL, $headers);
  }

  /**
   * POST HTTP request to the service.
   *
   * @param string $method
   *   The request method.
   * @param Array $query
   *   The query items.
   * @param Object $data
   *   The data items.
   * @param Array $headers
   *   The header items.
   *
   * @return Object http_request
   */
  private function POST($method, $query = array(), $data = NULL, $headers = array()) {
    return $this->push('POST', $method, $query, $data, $headers);
  }

  /**
   * PUT HTTP request to the service.
   *
   * @param string $method
   *   The request method.
   * @param Array $query
   *   The query items.
   * @param Object $data
   *   The data items.
   * @param Array $headers
   *   The header items.
   *
   * @return Object http_request
   */
  private function PUT($method, $query = array(), $data = NULL, $headers = array()) {
    return $this->send('PUT', $method, $query, array($data), $headers);
  }

  /**
   * DELETE HTTP request to the service.
   *
   * @param string $method
   *   The request method.
   * @param Array $query
   *   The query items.
   * @param Array $headers
   *   The header items.
   *
   * @return Object http_request
   */
  private function DELETE($method, $query = array(), $headers = array()) {
    return $this->send('DELETE', $method, $query, NULL, $headers);
  }

  /**
   * Send HTTP request to the service.
   *
   * @param String $http_method
   *   The request HTTP-method.
   * @param String $method
   *   The request method.
   * @param Array $query
   *   The query items.
   * @param String/Array $payload
   *   The payload.
   * @param Array $headers
   *   The header items.
   *
   * @return Object http_request
   */
  private function send($http_method, $method = '', $query = array(), $payload = NULL, $headers = array()) {
    // Build URL from REST endpoint and method.
    $url = $this->get_rest_endpoint() . '/' . trim($method, '/') . '.xml';
    if (!empty($query)) {
      // Build querystring from array.
      $querystring = http_build_query($query);
      $url = "{$url}?{$querystring}";
    }

    // If we're sending multiple documents we need to wrap them in <docs></docs>.
    if (is_array($payload) && !empty($payload)) {
      $payload = '<docs>' . implode($payload) . '</docs>';
    }

    // Extend default headers with overridden headers.
    $options = array(
      'method' => $http_method,
      'headers' => array_merge($this->default_headers(), $headers),
      'data' => $payload,
      'max_redirects' => 3,
      'timeout' => 30,
    );

    // Send HTTP request to the service.
    $results = drupal_http_request($url, $options);

    // If a problem is detected then log the results.
    if (isset($results->code) && $results->code != 200) {
      $messages = array(
        '@1' => $this->get_rest_endpoint(),
        '@2' => $results->code,
        '@3' => print_r($results, TRUE)
      );

      watchdog('fw_indexsvc', 'Error: Error for @1. Code: @2, Request: @3', $messages, WATCHDOG_ERROR);
    }

    return $results;
  }

  private function push() {
    $queue = new ExpireQueue($this);
    $queue->createItem(func_get_args());
    $this->send_queue();
  }

  /**
   * Send items in the queue.
   *
   * @param $amount The maximum number of items to send (defaults to 10).
   *
   * @return Integer Number of items sent.
   */
  private function send_queue($amount = FW_NUMBER_OF_ITEMS) {
    $count = 0;
    $queue = new ExpireQueue($this);
    if ($amount > 0 && $queue->numberOfItems() >= $amount) {
      $payload = array();
      $args;
      while($count++ < $amount && $item = $queue->claimItem()) {
        $args = $item->data;
        $payload[] = $item->data[3];
        $queue->deleteItem($item);
      }
      $this->send($args[0], $args[1], $args[2], $payload, $args[4]);
    }

    return $count;
  }

  /**
   * Check the queue for expired items.
   */
  public function check_queue($expire = 1) {
    $queue = new ExpireQueue($this);
    $this->send_queue($queue->expired($expire));

    // Remove all old queue items.
    if ($queue->numberOfItems() == 0) {
      $queue->deleteQueue();
    }
  }

  /**
   * Check status of current service for this handler.
   *
   * @return Array
   */
  public function check_status() {
    // Make sure we have an empty push queue.
    $this->check_queue();

    // Call the index service for a summary.
    $result = $this->GET('summary');
    if (isset($result)) {
      $count = $this->count($result);
    }
    //CN
    return (object) array(
        'state' => 'success',
        'title' => t('Success'),
        'path' => $this->endpoint,
        'message' => t('Connected to index service'),
        'count' => $count,
      );

    // If there where problems we want to handle that specially.
    if (isset($result->error) && $results->code != 200) {
      // Format and return the status object.
      return (object) array(
        'state' => 'danger',
        'title' => t('Error'),
        'path' => 'admin/reports/dblog',
        'message' => $this->get_rest_endpoint() . '/summary - ' . $result->error,
        'count' => $count,
      );
    } elseif (isset($result->data)) {
      // Parse XML-summary of the service.
      $xml = simplexml_load_string($result->data, 'SimpleXMLElement', LIBXML_NOCDATA);
      $status = (string) $xml->identStatus;

      // Format and return the status object.
      return (object) array(
        'state' => preg_match('/(running)/i', $status) ? 'success' : 'warning',
        'title' => t($status),
        'path' => $this->endpoint,
        'message' => t('Connected to index service'),
        'count' => $count,
      );
    }
  }

  /**
   * Create the xml structure for a field.
   *
   * @param string $type
   *   The data item type eg. "string", "int".
   * @param string $name
   *   The data item name.
   * @param string $content
   *   The data item content.
   *
   * @return Array XML Structure for the service.
   */
  public static function xml_structure($type, $name, $content) {

    return array(
      'type' => $type,
      'name' => $name,
      'content' => $content,
    );
  }

  /**
   * Get the document Id from an entity.
   *
   * @param Object $entity
   *  The entity object.
   *
   * @return String the document Id.
   */
  private function get_docId($entity, $type) {
    $id = entity_id($type, $entity);
    return "{$type}/{$id}";
  }

  /**
   * Build an array of fields for an entity.
   *
   * @param Object $entity
   *   The entity.
   * @param String $type
   *   The type of entity.
   * @param Array $exclude
   *   A list of names to exlude.
   *
   * @return Array Fields from this entity.
   */
  private function build_fields($entity, $type, $exclude = array(), $reset = FALSE) {
    $items = array();
    global $base_root;

    // DOCUMENT ID
    $docId = $this->get_docId($entity, $type);
    $items['docId'] = $this->xml_structure('string', 'docId', $docId);
    $items['entityType'] = $this->xml_structure('string', 'entity_type', $type);

    $url = call_user_func_array('url', array_merge(array(), entity_uri($type, $entity)));

    // Grab a metadata wrapper and add the fields.
    $wrapper = entity_metadata_wrapper($type, $entity);
    foreach ($wrapper as $field) {
      try {
        $items = array_merge($items, $this->get_field_value($field, $exclude));
      }
      catch (Exception $e) {
        watchdog('fw_indexsvc', 'EntityMetadataWrapperException (%field): %message', array('%message' => $e->getMessage(), '%field' => $field));
      }
    }

    // File references are not included through entity metadata wrapper, so let's add them.
    if ($type == 'node') {
      static $cached;
      // Set the cached fields fields for the first time.
      if (!isset($cached, $cached[$entity->type]) || $reset) {
        $all_file_fields = field_read_fields(array('type' => array('file', 'media', 'image')));
        $fields = field_info_instances('node', $entity->type);
        $cached[$entity->type] = array_intersect_key($all_file_fields, $fields);
      }
      foreach($cached[$entity->type] as $field_name => $field_info) {
        if (property_exists($entity, $field_name)) {
          $field_items = field_get_items('node', $entity, $field_name);
          if (isset($field_items) && !empty($field_items)) {
            foreach($field_items as $field_instance) {
              $items[$field_name] = $this->xml_structure('string', $field_name,
                'file/' . $field_instance['fid']);
              // Check if sources for files include specific file_fields. Then the
              // file insert won't be triggered because the file is saved before
              // the node. And will not be included due to the missing relation to
              // the node which is necessary for the indexing to be correct.
              if (isset($this->sources, $this->sources['file']) && !in_array('all', $this->sources['file'])) {
                $file_entity = entity_load_single('file', $field_instance['fid']);
                // If file was just added, then we want to trigger an entity_insert on the file (within 10mins).
                if ($file_entity->timestamp - strtotime('now') < 60 * 10) {
                  module_invoke('fw_indexsvc', 'entity_insert', $file_entity, 'file');
                }
              }
            }
          }
        }
      }
      //add custom fields from author
      if($wrapper->author->field_firstname->value()) {
        $items['firstname'] = $this->xml_structure('string', 'firstname', $wrapper->author->field_firstname->value()); 
      }
      if($wrapper->author->field_lastname->value()) {
        $items['lastname'] = $this->xml_structure('string', 'lastname', $wrapper->author->field_lastname->value()); 
      }

    }

    // Filter non empty items.
    $items = array_filter($items, array($this, 'filter_empty_value'));

    // Concatenate multivalue fields (Hydra).
    return $this->concat($items);
  }

  /**
   * Build fields out of a single field.
   *
   * @param EntityMetadataWrapper $field
   *   The field structure and values.
   * @param String $parent_name
   *   A name to prepend to all items.
   * @param Array $excluded_names
   *   A list of names to exlude.
   * @param Array $excluded_types
   *   A list of types to exlude.
   *
   * @return Array
   *   Fields discovered in or below this fields structure.
   */
  private function get_field_value(EntityMetadataWrapper $field, $parent_name = '', $excluded_names = array(), $excluded_types = array()) {
    $items = array();

    if (is_array($parent_name)) {
      $exclude = $parent_name;
      $parent_name = '';
    }

    $type_exclude = array_merge(array(
      'integer', 'boolean',
      'field_image', 'field_item_image',
      'list<file>', 'field_item_file', 'list<field_item_file>',
      'og_membership', 'list<og_membership>',
    ), $excluded_types);

    // Set which fields should be excluded.
    $name_exclude = array_merge(array(
      // Exlude to limit what fields we see.
      'nid', 'vid', 'is_new', 'status', 'promote', 'sticky', 'source', 'revision',
      'comment', 'comment_count', 'comment_count_new',
      // Exclude because we don't want them.
      'log', 'edit_url', 'format', 'duration',
      ), $excluded_names);

    $info = $field->info();
    $name = $info['name'];
    $machine_name = $name;

    if ($parent_name) {
      // If numeric name.
      if (preg_match('/^\d+$/', $info['name'])) {
        $name = "{$parent_name}";
        $machine_name = "{$parent_name}.[{$name}]";
      }
      else {
        $name = "{$parent_name}.{$name}";
      }
    }

    if (in_array($info['type'], $type_exclude) || in_array($info['name'], $name_exclude)) {
      return $items;
    }

    switch ($info['type']) {
      // DATE
      case 'date':
        $items[] = $this->xml_structure('string', $name, date('Y-m-d\TH:i:s\Z',$field->value()+date('Z',$field->value())));
        break;

      // TEXT.
      case 'token':
      case 'text':
        $items[] = $this->xml_structure('string', $name, $this->filter($field->value()));
        break;

      // URL.
      case 'uri':
        $items[] = $this->xml_structure('string', $name, $this->filter($field->value()));
        break;

      // USER/TERM.
      case 'user':
      case 'taxonomy_term':
        try {
          if (!is_null($field->value())) {
            $items[] = $this->xml_structure('string', $name, $this->filter($field->name->value()));
          }
        } catch(EntityMetadataWrapperException $e) {
          watchdog(
            'fwc_indexsvc',
            $name . ' - ' . $e->getMessage() . '<br /> '
              . 'See '  . __FUNCTION__ . '() <pre>'
              . $e->getTraceAsString() . '</pre>', NULL, WATCHDOG_ERROR
          );
        }
        break;

      // TEXT FORMATTED.
      case 'text_formatted':
        if (is_a($field, 'EntityStructureWrapper')) {
          $values = $field->value();
          if (is_array($values)) {
            // Use safe value if it has more content.
            if (isset($values['safe_value']) && strlen($values['safe_value']) >= strlen($values['value'])) {
              $value = $values['safe_value'];
            }
            else if (isset($values['value'])){
              $value = check_markup($values['value'], 'plain_text');
            }

            if (isset($value)) {
              // Add the value and and extract markup in separate fields.
              $items[] = $this->xml_structure('string', $name, $this->extract($value, $items));
            }

            // Add the summary if we can find it.
            if (isset($values['summary']) ) {
              $items[] = $this->xml_structure('string', $name, $this->filter($values['summary']));
            }
          }
          elseif (is_object($values)) {
            foreach ($values as $child) {
              // Recursively go through the structures values.
              foreach ($this->get_field_value($child, $name, $excluded_names) as $child_name => $child_item) {
                $items[] = $child_item;
              }
            }
          }
        }
        break;

      // LIST<taxonomy_term>.
      case 'list<taxonomy_term>':
        foreach ($field->value() as $delta => $term) {
          if (is_object($term)) {
            $items[] =
              $this->xml_structure('string', $name, $this->filter_name($term));
          }
        }
        break;

      // LIST<node>.
      case 'list<node>':
        foreach ($field->value() as $delta => $node) {
          if (is_object($node)) {
            $items[] =
              $this->xml_structure('string', $name, "node/{$node->nid}");
          }
        }
        break;

      // LIST<field_item_link>.
      case 'list<field_item_link>':
        foreach ($field->value() as $delta => $link) {
          $link['attributes'] = is_array($link['attributes']) ? $link['attributes'] : array();
          $attributes = array_merge(array('absolute' => TRUE), $link['attributes']);
          $items[] = $this->xml_structure('string', $name, $this->extract(l($link['title'], $link['url'], $attributes), $items));
        }
        break;

      // NODE.
      case 'node':
        if (is_object($field->value())) {
          $items[] = $this->xml_structure('string', $name, "node/{$field->value()->nid}");
        }
        break;

      // ENTITY.
      case 'entity':
        $entity = $field->value();
        $type = property_exists($entity, 'type') ? $entity->type : ($entity instanceof FieldCollectionItemEntity ? 'field_collection' : NULL);
        if ($type) {
          $wrapper = entity_metadata_wrapper($type, $entity);
          foreach ($wrapper as $entity_field) {
            try {
              $items = array_merge($items, $this->get_field_value($entity_field, $name, $excluded_names, array('uri', 'entity')));
            }
            catch (Exception $e) {
              watchdog('fw_indexsvc', 'EntityMetadataWrapperException (%field): %message', array('%message' => $e->getMessage(), '%field' => $field));
            }
          }
        }
        break;

      // STRUCT/FIELD_COLLECTION/LIST
      case 'struct':
      case 'field_collection_item':
      case !!preg_match('/^list</', $info['type']):
        foreach ($field as $child) {
          // Recursively go through the structures values.
          foreach ($this->get_field_value($child, $name, $excluded_names, array('uri', 'entity')) as $child_name => $child_item) {
            $items[] = $child_item;
          }
        }
        break;

      // ALL OTHER CASES.
      default:
        watchdog('notice', '%handler does not support fields of type %type.', array(
          '%handler' => (string) $this,
          '%type' => $info['type'],
          )
        );
        break;
    }

    return (array) $items;
  }

  /**
   * Filter out the text with optional tags.
   *
   * @param String $string
   *  The unfiltered string.
   * @param String $tags
   *  Example '<a> <h2>'.
   *
   * @return String of the string.
   */
  public static function filter($string, $tags = '') {
    $string = fwIndexSvc::decode($string);

    // Add spaces between tags except tags listed in $tags.
    $leave_tags = trim(preg_replace('/.*?([a-z1-6]+)>.*?/i', '$1|', $tags), '|');
    $string = preg_replace("/(<(?!\/?(?:{$leave_tags}))[^>]+?>)/i", ' $1 ', $string);

    // Remove all HTML tags except tags listed in $tags.
    $string = strip_tags($string, $tags);

    // Clean away unwanted characters and spaces.
    $string = fwIndexSvc::clean($string);

    if (strlen($tags) > 0 || preg_match('/[&<>]/', $string)) {
      // Wrap text with CDATA when we include tags.
      $string = fwIndexSvc::cdata($string);
    }

    return $string;
  }

  /**
   * Extract tags to
   *
   * @param String $string
   *  The unfiltered string.
   *
   * @return String CDATA.
   */
  public static function extract($string, &$items, $tags = '<h2> <h3> <h4> <h5> <h6> <strong> <em>') {
    // Refactor tags as an array of tags.
    $valid_tags = array();
    preg_match_all('|<([a-zA-Z1-6]+)>|i', $tags, $valid_tags);

    $string = fwIndexSvc::decode($string);

    // Create a DOM document and load the UTF-8 markup for the node.
    $dom = new DOMDocument();
    @$dom->loadHTML('<?xml encoding="UTF-8">' . $string);

    if (isset($valid_tags[1]) > 0) {
      // Render the markup from the wanted tags.
      foreach($valid_tags[1] as $tag) {
        foreach($dom->getElementsByTagName($tag) as $delta => $Node){
          if (strlen($Node->textContent) > 0) {
            $items["{$tag}.{$delta}.text"] = fwIndexSvc::xml_structure('string', "html.{$tag}.text", fwIndexSvc::filter($Node->textContent));
          }
        }
      }
    }

    return fwIndexSvc::filter($string);
  }

  /**
   * Wrap string as CDATA.
   *
   * @param String $string
   *  The unfiltered string.
   *
   * @return String CDATA.
   */
  public static function cdata($string) {
    return "<![CDATA[" . $string . "]]>";
  }

  /**
   * Decode text to UTF-8 and remove all html entities.
   *
   * @param String $string
   *  The unfiltered string.
   *
   * @return String of the string.
   */
  public static function decode($string) {
    if (mb_detect_encoding($string) != 'UTF-8') {
      $string = utf8_encode($string);
    }

    // Decode all &...; to normal characters.
    $string = preg_replace("/(&nbsp;)/", ' ', $string);
    $string = html_entity_decode($string);

    // Remove all remaining html-entities.
    $string = preg_replace("/(&[^;\W]+;)/", '', $string);

    return $string;
  }

  /**
   * Format long text as CDATA, filter out HTML and html-entities.
   *
   * @param String $string
   *  The unfiltered string.
   *
   * @return String of the string.
   */
  public static function clean($string) {

    // Remove multiple spaces.
    $string = preg_replace('/(\s{2,})/', ' ', $string);

    // Trim whitespaces from start and end of string.
    $string = preg_replace(array('/^([\s]+)/', '/([\s]+)$/'), '', $string);

    // Reject overly long 2 byte sequences, as well as characters above U+10000.
    $string = preg_replace('/[\x00-\x08\x10\x0B\x0C\x0E-\x19\x7F]' .
      '|[\x00-\x7F][\x80-\xBF]+' .
      '|([\xC0\xC1]|[\xF0-\xFF])[\x80-\xBF]*' .
      '|[\xC2-\xDF]((?![\x80-\xBF])|[\x80-\xBF]{2,})' .
      '|[\xE0-\xEF](([\x80-\xBF](?![\x80-\xBF]))|(?![\x80-\xBF]{2})|[\x80-\xBF]{3,})/S', '', $string);

    // Reject overly long 3 byte sequences and UTF-16 surrogates.
    $string = preg_replace('/\xE0[\x80-\x9F][\x80-\xBF]' .
      '|\xED[\xA0-\xBF][\x80-\xBF]/S', '', $string);

    // Reject invalid XML characters.
    $string = preg_replace('/[^\x{0009}\x{000a}\x{000d}\x{0020}-\x{D7FF}\x{E000}-\x{FFFD}]+/u', '', $string);

    return $string;
  }

  /**
   * Concat XML multivalue fields.
   *
   * @param Array $items
   *  The fields.
   *
   * @return Array Concatenated fields with same name.
   */
  private function concat($items) {
    $arr = array();
    foreach($items as $o) {
      if (isset($arr[$o['name']])) {
        $arr[$o['name']]['content'] .= ' ' . $o['content'];
      }
      else {
        $arr[$o['name']] = $o;
      }
    }
    return $arr;
  }

  /**
   * Check if entity of type is indexed.
   *
   * @param Object $entity
   * @param String $type
   *
   * @return Boolean
   */
  private function is_indexed($entity, $type) {
    return isset($this->sources[$type])
      && method_exists($this, "is_indexed_{$type}")
      && $this->{"is_indexed_{$type}"}($entity);
  }

  /**
   * Check if node is indexed.
   *
   * @param Object $entity
   *
   * @return Boolean
   */
  private function is_indexed_node($entity) {
    return in_array($entity->type, $this->sources['node']);
  }

  /**
   * Check if file is indexed.
   *
   * @param Object $entity
   *
   * @return Boolean
   */
  private function is_indexed_file($entity) {
    if (in_array('all', $this->sources['file'])) {
      return TRUE;
    }
    else {
      // Check if file is used in any of the bundles and fields we index.
      foreach($this->sources['file'] as $field_name) {
        $query = db_select('file_managed', 'fm');
        $query->join("field_data_{$field_name}", 'fi', "fm.fid = fi.{$field_name}_fid");
        $query->condition('fm.fid', $entity->fid);
        if (isset($this->sources, $this->sources['node'])) {
          $query->condition('fi.bundle', $this->sources['node'], 'IN');
        }
        $query->addExpression('COUNT(fid)', 'file_count');
        // Allow other modules to alter the query.
        drupal_alter('indexed_files', $query, $this);
        $result = $query->execute();
        if (($record = $result->fetchAssoc())) {
          return $record['file_count'] > 0;
        }
      }
    }
    return FALSE;
  }

  /**
   * Check if user is indexed.
   *
   * @param Object $entity
   *
   * @return Boolean
   */
  private function is_indexed_user($entity) {
    $rids = array_keys($entity->roles);
    foreach ($rids as $rid) {
      if(in_array($rid, $this->sources['user'])) {
        return TRUE;
      }
    }
    return FALSE;
  }

  /**
   * Filter out the name from an $object.
   *
   * @param Object $object
   *
   * @return String Name.
   */
  private function filter_name($object) {
    return !empty($object) && property_exists($object, 'name') ? $object->name : NULL;
  }

  /**
   * Filter out the name from an $object.
   *
   * @param Object $object
   *
   * @return String Name.
   */
  private function filter_empty_value($object) {
    return !(empty($object['content']) || preg_match('/(<\!\[CDATA\[\s*\]\]>)/', $object['content']));
  }
}
