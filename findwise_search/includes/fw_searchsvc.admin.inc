<?php
/**
 * Page callback: Search service settings
 *
 * @see findwise_search_menu()
 */
function fw_searchsvc_form($form, &$form_state) {

  $form['fw_searchsvc_name'] = array(
    '#type' => 'textfield',
    '#title' => t('Search service identifier name'),
    '#default_value' => variable_get('fw_searchsvc_name','default'),
    '#description' => t('The identifier of the search-service.'),
    '#required' => TRUE,
  );

  $form['fw_searchsvc_endpoint'] = array(
    '#type' => 'textfield',
    '#title' => t('Search service Endpoint'),
    '#default_value' => variable_get('fw_searchsvc_endpoint','http://localhost:8090'),
    '#description' => t('The absolute URL to the search-service.'),
    '#required' => TRUE,
  );

var_dump($form); exit;
  return system_settings_form($form);
}
