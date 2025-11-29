default_scss_files = files([
  '_colors-public.scss',
  '_colors.scss',
  '_common.scss',
  '_drawing.scss',
])

default_theme_variants = [
  'light',
  'dark',
  'hc',
  'hc-dark',
]

default_theme_deps = []

foreach variant: default_theme_variants
  default_theme_deps += custom_target('Default theme variant ' + variant,
    input: 'Default-@0@.scss'.format(variant),
    output: 'Default-@0@.css'.format(variant),
    command: [
      sassc, sassc_opts, '@INPUT@', '@OUTPUT@',
    ],
    depend_files: default_scss_files,
  )
endforeach
