<script setup>
import { ref } from 'vue'
import Checkbox from '../../components/Checkbox.vue'
import CommandTable from '../../components/CommandTable.vue'

const props = defineProps({
  platform: String,
  config: Object,
  globalPrepCmd: Array,
})

const config = ref(props.config)
const globalPrepCmd = ref(props.globalPrepCmd)

function addCmd() {
  let template = {
    do: '',
    undo: '',
  }

  if (props.platform === 'windows') {
    template = { ...template, elevated: false }
  }
  globalPrepCmd.value.push(template)
}

function removeCmd(index) {
  globalPrepCmd.value.splice(index, 1)
}

function handleCommandOrderChanged(newOrder) {
  // 更新命令顺序
  globalPrepCmd.value.splice(0, globalPrepCmd.value.length, ...newOrder)
}
</script>

<template>
  <div id="general" class="config-page">
    <div class="settings-grid">
      <!-- Sunshine Name -->
      <div class="settings-field">
        <label for="sunshine_name" class="form-label">{{ $t('config.sunshine_name') }}</label>
        <input
          type="text"
          class="form-control"
          id="sunshine_name"
          placeholder="Sunshine"
          v-model="config.sunshine_name"
        />
        <div class="form-text">{{ $t('config.sunshine_name_desc') }}</div>
      </div>

      <!-- Log Level -->
      <div class="settings-field">
        <label for="min_log_level" class="form-label">{{ $t('config.log_level') }}</label>
        <select id="min_log_level" class="form-select" v-model="config.min_log_level">
          <option value="0">{{ $t('config.log_level_0') }}</option>
          <option value="1">{{ $t('config.log_level_1') }}</option>
          <option value="2">{{ $t('config.log_level_2') }}</option>
          <option value="3">{{ $t('config.log_level_3') }}</option>
          <option value="4">{{ $t('config.log_level_4') }}</option>
          <option value="5">{{ $t('config.log_level_5') }}</option>
          <option value="6">{{ $t('config.log_level_6') }}</option>
        </select>
        <div class="form-text">{{ $t('config.log_level_desc') }}</div>
      </div>

      <!-- Sleep Mode -->
      <div class="settings-field">
        <label for="sleep_mode" class="form-label">{{ $t('config.sleep_mode') }}</label>
        <select id="sleep_mode" class="form-select" v-model="config.sleep_mode">
          <option value="0">{{ $t('config.sleep_mode_suspend') }}</option>
          <option value="1">{{ $t('config.sleep_mode_hibernate') }}</option>
          <option value="2">{{ $t('config.sleep_mode_away') }}</option>
        </select>
        <div class="form-text">{{ $t('config.sleep_mode_desc') }}</div>
      </div>
    </div>

    <!-- Global Prep Commands -->
    <div class="settings-panel mt-3">
      <label class="form-label">{{ $t('config.global_prep_cmd') }}</label>
      <div class="form-text">{{ $t('config.global_prep_cmd_desc') }}</div>
      <CommandTable
        class="mt-3"
        :commands="globalPrepCmd"
        :platform="platform"
        type="prep"
        @add-command="addCmd"
        @remove-command="removeCmd"
        @order-changed="handleCommandOrderChanged"
      />
    </div>

    <div class="settings-grid mt-3">

      <!-- Enable system tray -->
      <Checkbox
        container-class="settings-field settings-toggle-field"
        id="system_tray"
        locale-prefix="config"
        v-model="config.system_tray"
        default="true"
      ></Checkbox>
    </div>
  </div>
</template>
