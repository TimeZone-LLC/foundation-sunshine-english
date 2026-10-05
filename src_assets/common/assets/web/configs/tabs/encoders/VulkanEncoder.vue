<script setup>
import { ref } from 'vue'

const props = defineProps(['config'])
const config = ref(props.config)
</script>

<template>
  <div id="vulkan-encoder" class="config-page">
    <div class="mb-3">
      <label for="vk_tune" class="form-label">{{ $t('config.vk_tune') }}</label>
      <select id="vk_tune" v-model="config.vk_tune" class="form-select">
        <option value="0">{{ $t('auto') }}</option>
        <option value="1">{{ $t('config.vk_tune_hq') }}</option>
        <option value="2">{{ $t('config.vk_tune_ll') }}</option>
        <option value="3">{{ $t('config.vk_tune_ull') }}</option>
      </select>
      <div class="form-text">{{ $t('config.vk_tune_desc') }}</div>
    </div>

    <div class="mb-3">
      <label for="vk_rc_mode" class="form-label">{{ $t('config.vk_rc_mode') }}</label>
      <select id="vk_rc_mode" v-model="config.vk_rc_mode" class="form-select">
        <option value="0">{{ $t('auto') }}</option>
        <option value="1">{{ $t('config.vk_rc_cqp') }}</option>
        <option value="2">{{ $t('config.vk_rc_cbr') }}</option>
        <option value="4">{{ $t('config.vk_rc_vbr') }}</option>
      </select>
      <div class="form-text">{{ $t('config.vk_rc_mode_desc') }}</div>
    </div>

    <div class="mb-3">
      <label for="vk_quality" class="form-label">{{ $t('config.vk_quality') }}</label>
      <select id="vk_quality" v-model="config.vk_quality" class="form-select">
        <option value="speed">{{ $t('config.vk_quality_speed') }}</option>
        <option value="balanced">{{ $t('config.vk_quality_balanced') }}</option>
        <option value="quality">{{ $t('config.vk_quality_quality') }}</option>
      </select>
      <div class="form-text">{{ $t('config.vk_quality_desc') }}</div>
    </div>
  </div>
</template>
