/*
 * Outbound link data.
 *
 * The home page no longer advertises anything: the resource card that used to
 * render the official-site, tutorial, community and third-party-client lists is
 * gone, and so are those lists. What is left is only what still has a caller:
 *
 *   CLIENT_RESOURCES / FEATURED_RESOURCES / HARMONY_CLIENT_URL
 *       - the first-run Setup Wizard, which still points a new user at a client.
 *   LEGAL_RESOURCES
 *       - licence and third-party-notice targets, pinned by externalLinks.test.js.
 *
 * Do not reintroduce a list here without a screen that renders it.
 */
export const HARMONY_CLIENT_URL = 'https://github.com/AlkaidLab/moonlight-harmony'

const ALKAIDLAB_WEBSITE_URL = 'https://www.alkaidlab.com/'
const ALKAIDLAB_WEBSITE_ZH_URL = 'https://www.alkaidlab.cn/'
const VOIDLINK_APP_STORE_URL = 'https://apps.apple.com/us/app/voidlink-extreme/id6755103808'
const VOIDLINK_APP_STORE_ZH_URL = 'https://apps.apple.com/cn/app/voidlink/id6747717070'

export const CLIENT_RESOURCES = [
  {
    id: 'android-vplus',
    href: 'https://github.com/qiin2333/moonlight-vplus',
    icon: 'fab fa-android',
    titleKey: 'resource_card.android_vplus_title',
    description: 'Android / Android TV',
    variant: 'android',
  },
  {
    id: 'harmony-vplus',
    href: '#harmony-client',
    icon: 'fas fa-mobile-alt',
    titleKey: 'resource_card.harmony_client',
    description: 'HarmonyOS NEXT',
    variant: 'harmony',
    arrowIcon: 'fas fa-chevron-right',
    action: 'harmony',
  },
  {
    id: 'moonlight-desktop',
    href: 'https://github.com/qiin2333/moonlight-qt',
    icon: 'fas fa-desktop',
    titleKey: 'resource_card.moonlight_pc_title',
    description: 'Windows / macOS / Linux',
    variant: 'desktop',
  },
  {
    id: 'moonlight-macos',
    href: 'https://github.com/skyhua0224/moonlight-macos-enhanced',
    icon: 'fab fa-apple',
    titleKey: 'resource_card.moonlight_macos_enhanced',
    descriptionKey: 'resource_card.moonlight_macos_enhanced_desc',
    variant: 'apple',
  },
  {
    id: 'voidlink',
    href: VOIDLINK_APP_STORE_URL,
    zhHref: VOIDLINK_APP_STORE_ZH_URL,
    icon: 'fab fa-apple',
    titleKey: 'resource_card.voidlink_title',
    description: 'iOS / iPadOS',
    variant: 'apple',
  },
]

export const LEGAL_RESOURCES = [
  {
    id: 'license',
    href: 'https://github.com/AlkaidLab/foundation-sunshine/blob/master/LICENSE',
    icon: 'fas fa-file-alt',
    titleKey: 'resource_card.license',
    descriptionKey: 'resource_card.view_license',
    variant: 'accent',
  },
  {
    id: 'third-party-notice',
    href: 'https://github.com/AlkaidLab/foundation-sunshine/blob/master/NOTICE',
    icon: 'fas fa-exclamation-triangle',
    titleKey: 'resource_card.third_party_notice',
    descriptionKey: 'resource_card.third_party_desc',
    variant: 'accent',
  },
]

export const FEATURED_RESOURCES = [
  {
    id: 'alkaidlab',
    href: ALKAIDLAB_WEBSITE_URL,
    zhHref: ALKAIDLAB_WEBSITE_ZH_URL,
    imageSrc: '/images/logo-alkaidlab.png',
    imageAlt: 'AlkaidLab',
    titleKey: 'resource_card.official_website_title',
    descriptionKey: 'resource_card.official_website_desc',
    variant: 'accent',
  },
  {
    id: 'natpierce',
    href: 'https://docs.qq.com/aio/DRFVhWERDaFhKd1ZE',
    imageSrc: '/images/logo-natpierce.png',
    imageAlt: 'Natpierce',
    titleKey: 'resource_card.jiaoyuelian_title',
    descriptionKey: 'resource_card.jiaoyuelian_desc',
    variant: 'moonlink',
  },
]

export function resolveResourceText(translate, resource, field) {
  const key = resource[`${field}Key`]
  return key ? translate(key) : resource[field] || ''
}

export function resolveResourceHref(resource, locale) {
  return locale === 'zh' ? resource.zhHref || resource.href : resource.href
}
