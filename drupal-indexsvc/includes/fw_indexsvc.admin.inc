<?php

/**
 * @file
 * Admin forms and pages for the fw_indexsvc module.
 */

/**
 * Findwise IndexService administration page. Display a list of existing indentifiers.
 */
function fw_indexsvc_admin_page() {
  $idents = fw_indexsvc_get_idents(TRUE, TRUE);
  return theme('fw_indexsvc_admin_page', array('idents' => $idents));
}

/**
 * Theme the output for the main indentifier administration page.
 */
function theme_fw_indexsvc_admin_page($variables) {
  $idents = $variables['idents'];
  $output = '<p>' . t('This page lists all the <em>identifiers</em> that are
    currently defined on this system.', array(
      '@add-url' => url(FW_INDEXSVC_ADMIN_PATH . '/add'))) . '</p>';
  // Build out the list of normal, database listed identifiers.
  $rows = array();
  foreach ($idents as $ident) {
    $ops = theme('links', array(
      'links' => array(
        'ident_clear' => array(
          'title' => t('Clear'),
          'href' => FW_INDEXSVC_ADMIN_PATH . '/manage/'
          . strtolower($ident->name) . '/clear',
          'attributes' => array('class' => array('btn', 'btn-danger', 'btn-xs', 'btn-clear',
            $ident->status->state != 'success' ? ' disabled' : '')),
        ),
        'ident_reindex' => array(
          'title' => t('Upload'),
          'href' => FW_INDEXSVC_ADMIN_PATH . '/manage/'
          . strtolower($ident->name) . '/reindex',
          'attributes' => array('class' => array('btn', 'btn-warning', 'btn-xs', 'btn-upload',
            $ident->status->state != 'success' ? ' disabled' : '')),
        ),
        'ident_edit' => array(
          'title' => t('Edit'),
          'href' => FW_INDEXSVC_ADMIN_PATH . '/manage/'
          . strtolower($ident->name) . '/edit',
          'attributes' => array('class' => array('btn', 'btn-default', 'btn-xs', 'btn-edit')),
        ),
        'ident_delete' => array(
          'title' => t('Delete'),
          'href' => FW_INDEXSVC_ADMIN_PATH . '/manage/'
          . strtolower($ident->name) . '/delete',
          'attributes' => array('class' => array('btn', 'btn-danger', 'btn-xs', 'btn-delete')),
        ),
      ),
    ));
    $all_sources = "";
    foreach ($ident->sources as $type => $sources) {
      if (!empty($sources)) {
        if($type == 'user') {
          $user_roles = user_roles();
          foreach($sources as &$source) {
            $source = $user_roles[$source];
          };
        }
        $all_sources .= '<strong>' . $type . "</strong> (" . implode($sources, ', ') . ')<br />';
      }
    }
    $rows[] = array(
      'data' => array(
        '<strong>' . $ident->name . '</strong>',
        theme('fw_indexsvc_status', array('status' => $ident->status)),
        "{$ident->status->count->remote} / {$ident->status->count->local}",
        $all_sources,
        $ops,
      ),
    );
  }
  if (!$idents) {
    $rows[] = array(
      array('data' => t('No identifiers are currently defined.'), 'colspan' => 6),
    );
  }

  $header = array(t('Id'), t('Status'), t('Uploaded'), t('Sources'), t('Operations'));
  $output .= theme('table', array(
    'header' => $header,
    'rows' => $rows,
    'attributes' => array(
      'class' => array('indexsvc'),
    )
  ));

  return $output;
}

/**
 * Theming of the status field.
 */
function theme_fw_indexsvc_status($variables) {
  $status = $variables['status'];
  // Add bootstrap buttons css.
  drupal_add_css(drupal_get_path('module', 'fw_indexsvc') . '/fw_indexsvc.css', 'file');
  $attributes = array(
    'class' => 'btn btn-xs',
  );
  $attributes['class'] .= ' btn-' . $status->state;
  $attributes['title'] = $status->message;
  return l($status->title, $status->path, array(
    'attributes' => $attributes,
  ));
}

/**
 * Present a form for creating a new identifier.
 */
function fw_indexsvc_ident_form($form, &$form_state, $ident = NULL) {
  $name = '';
  if (!isset($ident)) {
    // Adding a new identifier.
    drupal_set_title(t('Add identifier'));
  } else {
    // Editing an existing identifier.
    $form['iid'] = array(
      '#type' => 'hidden',
      '#value' => $ident->iid,
    );
    drupal_set_title(t('Edit @title', array('@title' => $ident->name)));
    $name = $ident->name;
  }

  $form['name'] = array(
    '#type' => 'textfield',
    '#title' => t('Identifier name'),
    '#description' => t('The machine-name for this identifier. It may be up to 32 characters long and my only contain lowercase letters, underscores, and numbers. It will be used in URLs and in all API calls.'),
    '#maxlength' => 32,
    '#default_value' => isset($ident) ? $ident->name : '',
    '#required' => TRUE,
  );

  $form['endpoint'] = array(
    '#type' => 'textfield',
    '#title' => t('Endpoint URL'),
    '#description' => t('The full URL to the endpoint. Example: http://192.168.1.1:8080/indexws'),
    '#maxlength' => 225,
    '#default_value' => isset($ident) ? $ident->endpoint : '',
    '#required' => TRUE,
  );

/*  $form['linkdomain'] = array(
    '#type' => 'textfield',
    '#title' => t('URL domain for links'),
    '#description' => t('The domain that shall be used when linking to indexed content. Example: http://www.test.com'),
    '#maxlength' => 225,
    '#default_value' => variable_get('fw_indexsvc_'.$name.'_linkdomain', ''),
    '#required' => FALSE,
  );
 */
  $form['sources'] = array(
    '#type' => 'item',
    '#title' => t('Sources'),
    '#description' => t('The types of content this identifier will index.'),
  );
  
  // Add options for each type.
  $options = array();
  foreach (fw_indexsvc_fetch_type_options() as $type => $type_options) {
    foreach ($type_options as $option => $info) {
      $options[$type][$option] = $info->title . '<div class="description">' . $info->description . '</div>';
    }
  }

  // Add settings for each type.
  $settings = array();
  foreach (fw_indexsvc_fetch_type_settings(isset($ident, $ident->settings) ?
      $ident->settings : array()) as $type => $type_options) {
    foreach ($type_options as $option => $info) {
      $settings[$type][$option] = $info;
    }
  }

  // Build the fieldsets for all types.
  foreach (fw_indexsvc_fetch_type_definitions() as $type => $info) {
    if (!empty($options[$type])) {
      $form['sources'][$type] = array(
        '#type' => 'fieldset',
        '#title' => $info['title'],
        '#description' => $info['description'],
        '#collapsible' => TRUE,
        '#collapsed' => TRUE,
      );

      $form['sources'][$type]['options'] = array(
        '#type' => 'checkboxes',
        '#options' => $options[$type],
        '#default_value' => isset($ident, $ident->sources[$type]) ? $ident->sources[$type] : array(),
        '#parents' => array('sources', $type),
      );
      // Add settings for each type.
      if (isset($settings[$type])) {
        $form['sources'][$type]['settings'] = array_merge(array(
          '#tree' => TRUE,
          '#parents' => array('settings', $type),
          ), $settings[$type]);
      }
    }
  }

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Submit'),
  );

  return $form;
}

/**
 * Form validate handler.
 */
function fw_indexsvc_ident_form_validate($form, &$form_state) {
  // Ensure a safe machine name.
  if (!preg_match('/^[a-z_][a-z0-9_]*$/', $form_state['values']['name'])) {
    form_set_error('name', t('The identifier name may only contain lowercase letters, underscores, and numbers.'));
  }

  // Ensure the machine name is unique.
  if (!isset($form_state['values']['iid'])) {
    $ident = fw_indexsvc_get_ident($form_state['values']['name']);
    if (!empty($ident)) {
      form_set_error('name', t('Identifier names must be unique. This identifier name is already in use.'));
    }
  }
}

/**
 * Form submit handler.
 */
function fw_indexsvc_ident_form_submit($form, &$form_state) {
  $values = $form_state['values'];
  $iid = isset($values['iid']) ? $values['iid'] : 0;
  fw_indexsvc_update_identifiers($iid, $values['name'], $values['endpoint'], $values['sources'], $values['settings']);
  variable_set('fw_indexsvc_'.$values['name'].'_linkdomain', $values['linkdomain']);
  $form_state['redirect'] = FW_INDEXSVC_ADMIN_PATH;
}

/**
 * Delete identifier page.
 */
function fw_indexsvc_delete_confirm($form, &$form_state, $ident) {
  if (empty($ident)) {
    drupal_goto(FW_INDEXSVC_ADMIN_PATH);
  }

  $form['iid'] = array('#type' => 'value', '#value' => $ident->iid);

  return confirm_form($form,
    t('Are you sure you want to delete %title?', array(
      '%title' => $ident->name)),
    !empty($_GET['destination']) ?
      $_GET['destination'] : FW_INDEXSVC_ADMIN_PATH,
    t('This action cannot be undone.'),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Submit handler for form: fw_indexsvc_delete_confirm().
 */
function fw_indexsvc_delete_confirm_submit($form, &$form_state) {
  if ($form_state['values']['confirm']) {
    fw_indexsvc_delete_ident(NULL, $form_state['values']['iid']);
  }
  $form_state['redirect'] = FW_INDEXSVC_ADMIN_PATH;
}

/**
 * Upload identifier page.
 */
function fw_indexsvc_reindex($form, &$form_state, $ident) {
  if (empty($ident)) {
    drupal_goto(FW_INDEXSVC_ADMIN_PATH);
  }

  $form['iid'] = array('#type' => 'value', '#value' => $ident->iid);

  return confirm_form($form,
    t('Are you sure you want to do a full upload to %title?', array(
      '%title' => $ident->name)),
    !empty($_GET['destination']) ?
      $_GET['destination'] : FW_INDEXSVC_ADMIN_PATH,
    t('This action can\'t be aborted.'),
    t('Continue'),
    t('Cancel')
  );
}

/**
 * Submit handler for form: fw_indexsvc_reindex().
 */
function fw_indexsvc_reindex_submit($form, &$form_state) {
  $ident = fw_indexsvc_get_ident(NULL, $form_state['values']['iid']);
  batch_set(fw_indexsvc_create_reindex_batch($ident));
  $form_state['redirect'] = FW_INDEXSVC_ADMIN_PATH;
}

/**
 * Clear identifier page.
 */
function fw_indexsvc_clear($form, &$form_state, $ident) {
  if (empty($ident)) {
    drupal_goto(FW_INDEXSVC_ADMIN_PATH);
  }

  $form['iid'] = array('#type' => 'value', '#value' => $ident->iid);

  return confirm_form($form,
    t('Are you sure you want to clear the index on %title?', array(
      '%title' => $ident->name)),
    !empty($_GET['destination']) ?
      $_GET['destination'] : FW_INDEXSVC_ADMIN_PATH,
    t('This action can\'t be aborted.'),
    t('Continue'),
    t('Cancel')
  );
}

/**
 * Submit handler for form: fw_indexsvc_clear().
 */
function fw_indexsvc_clear_submit($form, &$form_state) {
  $ident = fw_indexsvc_get_ident(NULL, $form_state['values']['iid']);
  $idents = array(
    $ident->name => $ident,
  );
  $handlers = fw_indexsvc_get_handlers(NULL, $idents);
  // Get the unique handlers.
  $handlers = array_unique(call_user_func_array('array_merge', $handlers));
  foreach ($handlers as $handler) {
    if (method_exists($handler, 'clear')) {
      if ($handler->clear()) {
        drupal_set_message(t("The index for %name is now clearing.", array(
          '%name' => (string) $handler,
        )));
      }
    }
  }
  $form_state['redirect'] = FW_INDEXSVC_ADMIN_PATH;
}
