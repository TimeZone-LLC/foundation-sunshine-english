<script setup>
import { ref } from 'vue'

const props = defineProps(['config'])
const config = ref(props.config)
</script>

<template>
  <div id="vaapi-encoder" class="config-page">
    <div class="mb-3">
      <label for="vaapi_rc" class="form-label">{{ $t('config.vaapi_rc') }}</label>
      <select id="vaapi_rc" v-model="config.vaapi_rc" class="form-select">
        <option value="auto">{{ $t('auto') }}</option>
        <option value="avbr">{{ $t('config.vaapi_rc_avbr') }}</option>
        <option value="vbr">{{ $t('config.vaapi_rc_vbr') }}</option>
        <option value="cbr">{{ $t('config.vaapi_rc_cbr') }}</option>
        <option value="cqp">{{ $t('config.vaapi_rc_cqp') }}</option>
        <option value="icq">{{ $t('config.vaapi_rc_icq') }}</option>
        <option value="qvbr">{{ $t('config.vaapi_rc_qvbr') }}</option>
      </select>
      <div class="form-text">{{ $t('config.vaapi_rc_desc') }}</div>
    </div>

    <div class="mb-3">
      <label for="vaapi_blbrc" class="form-label">{{ $t('config.vaapi_blbrc') }}</label>
      <select id="vaapi_blbrc" v-model="config.vaapi_blbrc" class="form-select">
        <option value="disabled">{{ $t('_common.disabled_def') }}</option>
        <option value="enabled">{{ $t('_common.enabled') }}</option>
      </select>
      <div class="form-text">{{ $t('config.vaapi_blbrc_desc') }}</div>
    </div>

    <div class="mb-3">
      <label for="vaapi_strict_rc_buffer" class="form-label">{{ $t('config.vaapi_strict_rc_buffer') }}</label>
      <select id="vaapi_strict_rc_buffer" v-model="config.vaapi_strict_rc_buffer" class="form-select">
        <option value="disabled">{{ $t('_common.disabled_def') }}</option>
        <option value="enabled">{{ $t('_common.enabled') }}</option>
      </select>
      <div class="form-text">{{ $t('config.vaapi_strict_rc_buffer_desc') }}</div>
    </div>

    <div class="mb-3">
      <label for="vaapi_quality" class="form-label">{{ $t('config.vaapi_quality') }}</label>
      <select id="vaapi_quality" v-model="config.vaapi_quality" class="form-select">
        <option value="auto">{{ $t('auto') }}</option>
        <option value="speed">{{ $t('config.vaapi_quality_speed') }}</option>
        <option value="balanced">{{ $t('config.vaapi_quality_balanced') }}</option>
        <option value="quality">{{ $t('config.vaapi_quality_quality') }}</option>
      </select>
      <div class="form-text">{{ $t('config.vaapi_quality_desc') }}</div>
    </div>
  </div>
</template>
