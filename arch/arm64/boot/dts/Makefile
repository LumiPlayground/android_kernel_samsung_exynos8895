dts-dirs += al
dts-dirs += altera
dts-dirs += amd
dts-dirs += amlogic
dts-dirs += apm
dts-dirs += arm
dts-dirs += broadcom
dts-dirs += cavium
dts-dirs += exynos
dts-dirs += freescale
dts-dirs += hisilicon
dts-dirs += marvell
dts-dirs += mediatek
dts-dirs += nvidia
dts-dirs += qcom
dts-dirs += renesas
dts-dirs += rockchip
dts-dirs += socionext
dts-dirs += sprd
dts-dirs += xilinx
dts-dirs += lg
dts-dirs += zte

subdir-y          := $(dts-dirs)

always            := $(DTB_LIST)

targets += dtbs
dtbs: $(addprefix $(obj)/, $(DTB_LIST))

clean-files       := exynos/*.dtb *.dtb*
