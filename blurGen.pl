#!perl

use strict;

sub fact
{
  my $value = shift;
  if ($value <= 1)
  {
    return 1;
  }
  return $value*fact($value-1);
}

sub nCc
{
  my $n = shift;
  my $k = shift;
  
  my $n0 = fact($n);
  my $k0 = fact($k);
  my $nk = fact($n-$k);
  
  return $n0 / ($k0 * $nk);
}

sub getCoefficients
{
  my $n = shift;
  if (($n % 2) == 1)
  {
    $n++;
  }
  
  my $coeffs = [];
  my $width = $n+1;
  
  for (my $i=0; $i<$width/2; ++$i)
  {
    $coeffs->[$i] = nCc($n, $i);
  }

  @{$coeffs} = reverse(@{$coeffs});
  
  my $sum = 0;
  for (my $i=0; $i<$width; ++$i)
  {
    $sum += nCc($n, $i);
  }
  
  map { $_ /= $sum } @{$coeffs};
  map { $_ = sprintf("%.8f", $_); } @{$coeffs};
  
  return $coeffs;
}

sub generateBlurKernel
{
  my $kernelWidth = shift;
  my $coefficients = getCoefficients($kernelWidth);

  printf("__kernel void blur2d_%sx(__global float* input,\n", $kernelWidth);
  printf("                         __global float* output,\n");
  printf("                         __global unsigned int* mask,\n");
  printf("                         unsigned int width, unsigned int height)\n");
  
  printf("{\n");
  printf("    const float coefficients[] = { %sf };\n", join('f, ', @{$coefficients}));
  printf("    const int num_coeffs = %d;\n", scalar @{$coefficients});
  
print <<'EOF'
    const int index = get_global_id(0);
    const int pixel_r = (int) (index / width);
    const int pixel_c = index - (pixel_r*width);
    
    if (bitvector_get(mask, index))
    {
        float val = input[index]*coefficients[0];
        for (int j=1; j<num_coeffs; ++j)
        {
            int c_next = pixel_c+j;
            if (c_next > width-1 || !bitvector_get(mask, pixel_r*width+c_next))
                c_next = pixel_c;
            int c_prev = pixel_c-j;
            if (c_prev < 0 || !bitvector_get(mask, pixel_r*width+c_prev))
                c_prev = pixel_c;
            
            val += input[pixel_r*width+c_next]*coefficients[j];
            val += input[pixel_r*width+c_prev]*coefficients[j];
        }
        output[index] = val;
    }
    else
    {
        output[index] = input[index];
    }
}
EOF
    ;

  printf("__kernel void blur2d_%sy(__global float* input,\n", $kernelWidth);
  printf("                         __global float* output,\n");
  printf("                         __global unsigned int* mask,\n");
  printf("                         unsigned int width, unsigned int height)\n");

  printf("{\n");
  printf("    const float coefficients[] = { %sf };\n", join('f, ', @{$coefficients}));
  printf("    const int num_coeffs = %d;\n", scalar @{$coefficients});

print <<'EOF'
    const int index = get_global_id(0);
    const int pixel_r = (int) (index / width);
    const int pixel_c = index - (pixel_r*width);
    
    if (bitvector_get(mask, index))
    {
        float val = input[index]*coefficients[0];
        for (int j=1; j<num_coeffs; ++j)
        {
            int r_next = pixel_r+j;
            if (r_next > height-1 || !bitvector_get(mask, r_next*width+pixel_c))
                r_next = pixel_r;
            int r_prev = pixel_r-j;
            if (r_prev < 0 || !bitvector_get(mask, r_prev*width+pixel_c))
                r_prev = pixel_r;
            
            val += input[r_prev*width+pixel_c]*coefficients[j];
            val += input[r_next*width+pixel_c]*coefficients[j];
        }
        output[index] = val;
    }
    else
    {
        output[index] = input[index];
    }
}
EOF
}

my $max_kernel_width = $ARGV[0];
for (my $i=1; $i<$max_kernel_width; ++$i)
{
  if (($i%2) == 1)
  {
    generateBlurKernel($i);
  }
}
