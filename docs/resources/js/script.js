// Link to the slider and image element
const slider = document.getElementById('slider');
const sliderImage = document.getElementById('slider-image');

// Generate filenames programmatically
const imageFolder = "resources/images/optim/traj_evolution/";
const imageCount = 40; // Total number of images in folder
const optCount = 5;
const images = [];

for (let i = 0; i < imageCount; i++) {
    if (i < optCount) {
        for (let opt_id = 0; opt_id < optCount; opt_id++) {
            const filename = `grid_${i.toString().padStart(4, "0")}_opt_idx_${opt_id.toString().padStart(4, "0")}.png`;
            images.push(filename);
        }
    }
    const filename = `grid_${i.toString().padStart(4, "0")}_opt_idx_0000.png`;
    images.push(filename);
}

// Set the slider range dynamically based on the number of images
slider.max = images.length - 1;

// Update image based on slider value
function updateImage(index) {
  const imagePath = imageFolder + images[index];
  console.log("Slider Value:", index);
  console.log("Image Path:", imagePath);
  slider.value = index; // Sync the slider with the current index
  sliderImage.src = imagePath;
}

// Autoplay functionality
let autoplayInterval; // Store the interval reference
let currentIndex = 0; // Start from the first image

function startAutoplay() {
  autoplayInterval = setInterval(() => {
    if (currentIndex === images.length - 1) {
      // Pause for 2 seconds before restarting
      stopAutoplay(); // Stop the current interval
      setTimeout(() => {
        currentIndex = 0; // Reset to the first image
        updateImage(currentIndex); // Update to the first image
        startAutoplay(); // Restart autoplay
      }, 2500); // Pause for 2.5 seconds
    } else {
      currentIndex = (currentIndex + 1) % images.length; // Increment index
      updateImage(currentIndex);
    }
  }, 100); // Change image every 100ms
}

function stopAutoplay() {
  clearInterval(autoplayInterval);
}

// Start autoplay
startAutoplay();

// Pause autoplay when the cursor is over the slider
slider.addEventListener('mouseenter', () => {
  console.log("Paused autoplay because the cursor is over the slider");
  stopAutoplay();
});

// Resume autoplay when the cursor leaves the slider
slider.addEventListener('mouseleave', () => {
  console.log("Resumed autoplay because the cursor left the slider");
  startAutoplay();
});

// Manual slider control (update image when user interacts)
slider.addEventListener('input', () => {
  currentIndex = parseInt(slider.value, 10); // Update current index
  updateImage(currentIndex); // Update the image
});;



// // Link to the slider and image element
// const slider = document.getElementById('slider');
// const sliderImage = document.getElementById('slider-image');

// // Generate filenames programmatically
// const imageFolder = "resources/images/optim/traj_evolution/";
// const imageCount = 40; // Total number of images in folder
// const images = [];

// for (let i = 0; i < imageCount; i++) {
//   const filename = `grid_${i.toString().padStart(4, "0")}_opt_idx_0000.png`;
//   images.push(filename);
// }

// // Set the slider range dynamically based on the number of images
// slider.max = images.length - 1;

// const debugOutput = document.createElement('div');
// document.body.appendChild(debugOutput);
// // Event listener for the slider to update the image
// slider.addEventListener('input', () => {
//     const currentImageIndex = slider.value;
//     const imagePath = imageFolder + images[currentImageIndex];
//     // pass iamge to console to debug output on screen
//     console.log("Slider Value:", currentImageIndex); // Logs the current slider value
//     console.log("Image Path:", imagePath); // Logs the full path
//     // pass current image to slider
//     sliderImage.src = imagePath;

//   });