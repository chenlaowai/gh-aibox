<template>
    <div class="tips">人工智能也会犯错，请核查重要预警信息</div>
</template>

<script setup lang='ts'>

</script>

<style scoped>
.tips {
    position: absolute;
    bottom: 50px;
    left: 0;
    right: 0;
    z-index: 500;
    display: flex;
    justify-content: center;
    align-items: center;
    margin: 0 auto;
    width: 640px;
    height: 40px;
    background: linear-gradient(90deg, rgba(88, 135, 255, 0) 1%, rgba(88, 135, 255, 0.2116) 36%, rgba(122, 153, 255, 0.3) 50%, rgba(88, 135, 255, 0.2106) 65%, rgba(88, 135, 255, 0) 100%);
    border-radius: 0px 0px 0px 0px;
    border: 1px solid;
    border-image: linear-gradient(90deg, rgba(255, 255, 255, 0), rgba(207.61061489582062, 230.51549702882767, 255, 0.4000000059604645), rgba(208.0000028014183, 231.00000143051147, 255, 0)) 1 1;
    color: rgba(255, 255, 255, 0.8);
    font-size: 14px;
}
</style>