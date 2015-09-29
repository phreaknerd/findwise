jQuery(function(){
  jQuery('a.querysuggestion').bind('click', function( e ) {
    alert(jQuery(this).text());
    jQuery('input.findwise-search').value(jQuery(this).text());
    e.preventDefault();
  });
  jQuery("input.findwise-search").keyup(function( event ){
      if(this.value.length >= 3) {
        jQuery.getJSON('/fw_search/complete?query=' + this.value, function(data){
          jQuery('div#querycomplete').hide();
          jQuery('div#querycomplete').html('');
          if(data.length > 0) {
            jQuery.each(data, function(i, val) {
              jQuery('<a/>', { href: "#", text: '"' + val + '"', style: "display: block;"}).bind('click', function(e){
                jQuery('input.findwise-search').val(jQuery(this).text());
                jQuery('#search-form').submit();
                e.preventDefault();
              }).appendTo('div#querycomplete');
            });
            jQuery('div#querycomplete').show();
          }
        });
      }
      else {
          jQuery('div#querycomplete').hide();
      }
  });
});
