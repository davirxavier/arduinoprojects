import minify_html

with open('../config_page.html', encoding="utf-8") as f:
    read_data = f.read()
    minified = minify_html.minify(read_data, minify_css=True, minify_js=True)

with open('../config-html.h', 'w') as f:
    f.write('''#ifndef DX_ESP_CONFIG_PAGE_HTML_H
#define DX_ESP_CONFIG_PAGE_HTML_H
const char ESP_CONFIG_HTML[] PROGMEM = "%s";
#endif''' % minified.replace('\\', '\\\\').replace('"', '\\"'))

print('Written new html');

