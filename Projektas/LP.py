from PIL import Image
import math
import multiprocessing
from multiprocessing import Pool
from functools import partial
from contextlib import contextmanager
from timeit import default_timer as timer


# Fills a part of the given pixels with the pixels of the given image.
def fill_pixels_from_image(pixels, indexes, image):
    other_pixels = image.load()
    for i in range(indexes[0][0], indexes[1][0] + 1):
        for j in range(indexes[0][1], indexes[1][1] + 1):
            pixels[i,j] = other_pixels[i - indexes[0][0] ,j - indexes[0][1]]

# Returns the RGB value of the pixel of the given index by calculating the average RGB values of it and it's neighbors.
def smooth_pixel(i, j, pixels, size):
    neighbors_sum = [0, 0, 0]
    # Add RGB values of the left neighbor
    if i - 1 < 0: neighbors_sum = [x + y for x, y in zip(neighbors_sum, pixels[i, j])]
    else: neighbors_sum = [x + y for x, y in zip(neighbors_sum, pixels[i - 1, j])]
    # Add RGB values of the right neighbor
    if i + 1 >= size[0]: neighbors_sum = [x + y for x, y in zip(neighbors_sum, pixels[i, j])]
    else: neighbors_sum = [x + y for x, y in zip(neighbors_sum, pixels[i + 1, j])]
    # Add RGB values of the upper neighbor
    if j - 1 < 0: neighbors_sum = [x + y for x, y in zip(neighbors_sum, pixels[i, j])]
    else: neighbors_sum = [x + y for x, y in zip(neighbors_sum, pixels[i, j - 1])]
    # Add RGB values of the lower neighbor
    if j + 1 >= size[1]: neighbors_sum = [x + y for x, y in zip(neighbors_sum, pixels[i, j])]
    else: neighbors_sum = [x + y for x, y in zip(neighbors_sum, pixels[i, j + 1])]
    # Calculate the average RGB value and return it
    return [math.ceil((x + y) / 5) for x,y in zip(pixels[i,j], neighbors_sum)]

# Calculates and returns the given area of pixels of the new image by smoothing the given image.
def smooth(indexes, image):
    # Get pixels and size of the image
    pixels = image.load()
    size = image.size
    # Create a new image only for the given area
    new_image = Image.new("RGB", (indexes[1][0] - indexes[0][0] + 1, indexes[1][1] - indexes[0][1] + 1))
    new_pixels = new_image.load()

    # Smooth every pixel of the area
    for i in range(indexes[0][0], indexes[1][0] + 1):
        for j in range(indexes[0][1], indexes[1][1] + 1):
            new_pixels[i - indexes[0][0], j - indexes[0][1]] = tuple(smooth_pixel(i,j,pixels,size))

    # Return the new image and the area to reconstruct the whole image
    return [indexes, new_image]

# Set arguments to the pool
@contextmanager
def poolcontect(*args, **kwargs):
    pool = multiprocessing.Pool(*args, **kwargs)
    yield pool
    pool.terminate()



img_array=['64px.png','128px.png','256px.png','512px.png','1024px.png','2048px.png','4096px.png']

input= 'data/'+ img_array[6]

num_processes=55

output='result.png'


# Loads image from input, applys blur effect and saves it to output.
    # Load image
image = Image.open(input)
size = image.size

arguments = [] # Initialize the array that will hold the arguments for all processes
no_rows = math.ceil(size[0] / num_processes) # Set the number of rows each process will process
# Set the area of the new image each process will calculate
for i in range(0, size[0], no_rows):
    # Set upper left index
    start_index = [i, 0]
    # Set lower right index (if there are no more rows, set the last position as the last index)
    if i + no_rows > size[0] : end_index = [size[0] - 1, size[1] - 1]
    else : end_index = [i + no_rows - 1, size[1] - 1]
    arguments += [[start_index, end_index]]

# Map areas to functions and calculate the new pixels    

results = []

start = timer()

if __name__ == "__main__":
    with Pool(processes=num_processes) as pool:
        results += pool.map(partial(smooth, image=image), arguments)
    pool.close()

    end= timer()
    print("procesu skaicius: "+ str(num_processes))
    print("paveiksliukas: "+ input)
    print("Sugaista laiko s: " + str(round(end - start,5)))


    # Create a new image and fill it with the pixels of the results for each area
new_image = Image.new("RGB", (size[0], size[1]))
new_pixels = new_image.load()
for result in results:
    fill_pixels_from_image(new_pixels, result[0], result[1])

# Save new image
new_image.save(output, "PNG")
