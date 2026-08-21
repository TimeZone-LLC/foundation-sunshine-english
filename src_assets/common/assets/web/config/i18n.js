import {createI18n} from "vue-i18n";

// English is the only supported locale, so it is bundled statically.
import en from '../public/assets/locale/en.json'

export default async function() {
    document.querySelector('html').setAttribute('lang', 'en');
    const i18n = createI18n({
        legacy: false, // Composition API mode
        locale: 'en', // the WebUI is English-only
        fallbackLocale: 'en', // set fallback locale
        messages: {
            en
        },
        globalInjection: true, // allow $t to be used in templates
        warnHtmlMessage: false, // disable HTML message warnings (trusted translations are rendered with v-html)
    })
    return i18n;
}
