# (size-m, size-n, size-p, warm-cache, prefetch)
#  ^prefetch write block

# the original setting
#TESTS +=  $(call test-name,128,256,256,no,yes)

# cache resident setting
TESTS +=  $(call test-name,256,256,256,yes,no)

# cache non resident setting
TESTS +=  $(call test-name,512,512,512,no,yes)
TESTS +=  $(call test-name,512,512,512,no,no)
